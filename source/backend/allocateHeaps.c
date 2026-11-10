#include <3ds/types.h>
#include <3ds/svc.h>
#include <3ds/allocator/mappable.h>
#include <3ds/env.h>
#include <3ds/os.h>
#include <3ds/result.h>

extern char* fake_heap_start;
extern char* fake_heap_end;

u32 __ctru_heap = 0;
u32 __ctru_linear_heap = 0;

u32 __ctru_heap_size = 0;
u32 __ctru_linear_heap_size = 0;

static void debug_log_u32(const char* label, u32 val) {
	char buf[80];
	char* p = buf;
	while (*label && (p - buf) < 50) *p++ = *label++;
	*p++ = ':';
	*p++ = ' ';
	*p++ = '0';
	*p++ = 'x';
	for (int i = 7; i >= 0; i--) {
		u32 nib = (val >> (i * 4)) & 0xF;
		*p++ = (nib < 10) ? ('0' + nib) : ('A' + nib - 10);
	}
	*p++ = '\n';
	*p = '\0';
	svcOutputDebugString(buf, p - buf);
}

void __system_allocateHeaps(void) {
	Result rc;

	// Retrieve handle to the resource limit object for our process
	Handle reslimit = 0;
	rc = svcGetResourceLimit(&reslimit, CUR_PROCESS_HANDLE);
	if (R_FAILED(rc))
		svcBreak(USERBREAK_PANIC);

	// Retrieve information about total/used commit memory
	s64 maxCommit = 0, currentCommit = 0;
	ResourceLimitType reslimitType = RESLIMIT_COMMIT;
	svcGetResourceLimitLimitValues(&maxCommit, reslimit, &reslimitType, 1);
	svcGetResourceLimitCurrentValues(&currentCommit, reslimit, &reslimitType, 1);
	svcCloseHandle(reslimit);

	// Calculate remaining free memory available for allocation
	u32 remaining = 0;
	if (maxCommit > currentCommit) {
		remaining = (u32)(maxCommit - currentCommit) & ~0xFFF;
	}

	debug_log_u32("[HEAP] maxCommit", (u32)maxCommit);
	debug_log_u32("[HEAP] currentCommit", (u32)currentCommit);
	debug_log_u32("[HEAP] remaining", remaining);

	if (remaining < (4 << 20)) {
		svcBreak(USERBREAK_PANIC);
	}

	// Target 12 MB for logic/standard heap (malloc/Lua/JSON) + 512KB safety margin for page tables.
	// ALL THE REST of available RAM is allocated to contiguous Linear Heap (textures/audio)
	// so the engine can load as many character spritesheets, stages, and audio files as possible!
	u32 reserved_for_heap = (12 << 20) + (512 * 1024); // 12 MB heap + 512 KB kernel margin
	u32 target_linear = 0;
	if (remaining > (reserved_for_heap + (4 << 20))) {
		target_linear = (remaining - reserved_for_heap) & ~0xFFF;
	} else {
		target_linear = ((remaining / 2) & ~0xFFF);
	}

	debug_log_u32("[HEAP] target_linear", target_linear);

	// 1. Step-down allocation loop for Linear Heap
	__ctru_linear_heap = 0;
	__ctru_linear_heap_size = 0;
	while (target_linear >= (12 << 20)) {
		rc = svcControlMemory(&__ctru_linear_heap, 0x0, 0x0, target_linear, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);
		if (R_SUCCEEDED(rc)) {
			__ctru_linear_heap_size = target_linear;
			break;
		}
		target_linear -= (2 << 20);
	}

	// Fallback if needed
	if (R_FAILED(rc) || __ctru_linear_heap == 0) {
		target_linear = (10 << 20);
		while (target_linear >= (4 << 20)) {
			rc = svcControlMemory(&__ctru_linear_heap, 0x0, 0x0, target_linear, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);
			if (R_SUCCEEDED(rc)) {
				__ctru_linear_heap_size = target_linear;
				break;
			}
			target_linear -= (1 << 20);
		}
	}

	debug_log_u32("[HEAP] linear_rc", (u32)rc);
	debug_log_u32("[HEAP] linear_heap_size", __ctru_linear_heap_size);

	if (R_FAILED(rc) || __ctru_linear_heap == 0) {
		svcBreak(USERBREAK_PANIC);
	}

	// 2. Allocate the Application Heap with remaining free memory.
	// Leave a 512KB safety headroom for kernel page tables so commit limit is never breached.
	u32 heap_target = 0;
	if (remaining > __ctru_linear_heap_size) {
		heap_target = (remaining - __ctru_linear_heap_size) & ~0xFFF;
		if (heap_target > (512 * 1024)) {
			heap_target -= (512 * 1024);
		}
	}

	debug_log_u32("[HEAP] heap_target", heap_target);

	// Step-down loop for Application Heap:
	// If the kernel cannot map the entire remaining block, step down in 256KB increments until it succeeds.
	__ctru_heap = 0;
	__ctru_heap_size = 0;
	while (heap_target >= (4 << 20)) {
		rc = svcControlMemory(&__ctru_heap, OS_HEAP_AREA_BEGIN, 0x0, heap_target, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);
		if (R_SUCCEEDED(rc)) {
			__ctru_heap_size = heap_target;
			break;
		}
		heap_target -= (256 * 1024);
	}

	debug_log_u32("[HEAP] heap_rc", (u32)rc);
	debug_log_u32("[HEAP] __ctru_heap_size", __ctru_heap_size);

	if (R_FAILED(rc) || __ctru_heap == 0) {
		svcBreak(USERBREAK_PANIC);
	}

	// Initialize mappable allocator
	mappableInit(OS_MAP_AREA_BEGIN, OS_MAP_AREA_END);

	// Setup newlib heap
	fake_heap_start = (char*)__ctru_heap;
	fake_heap_end = fake_heap_start + __ctru_heap_size;
}
