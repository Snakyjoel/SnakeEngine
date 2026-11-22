import os
import sys

def sync_clean(assets_dir, romfs_dir):
    if not os.path.exists(romfs_dir):
        return

    # Valid extensions that assets get converted into
    # assets .t3s -> romfs .t3x
    # assets .ttf / .otf -> romfs .bcfnt
    # assets .ogg -> romfs .adp
    for root, dirs, files in os.walk(romfs_dir, topdown=False):
        rel_root = os.path.relpath(root, romfs_dir)
        matching_asset_dir = os.path.join(assets_dir, rel_root) if rel_root != "." else assets_dir

        for f in files:
            romfs_file = os.path.join(root, f)
            name, ext = os.path.splitext(f)

            # Check if source asset exists
            source_exists = False
            if ext == ".t3x":
                source_exists = os.path.exists(os.path.join(matching_asset_dir, name + ".t3s"))
            elif ext == ".bcfnt":
                source_exists = os.path.exists(os.path.join(matching_asset_dir, name + ".ttf")) or \
                                os.path.exists(os.path.join(matching_asset_dir, name + ".otf"))
            elif ext == ".adp":
                source_exists = os.path.exists(os.path.join(matching_asset_dir, name + ".adp")) or \
                                os.path.exists(os.path.join(matching_asset_dir, name + ".ogg"))
            else:
                source_exists = os.path.exists(os.path.join(matching_asset_dir, f))

            if not source_exists:
                try:
                    os.remove(romfs_file)
                    print(f"Purged orphan RomFS file: {os.path.relpath(romfs_file, romfs_dir)}")
                except Exception as e:
                    pass

        # Remove empty directories in romfs
        if not os.listdir(root) and root != romfs_dir:
            try:
                os.rmdir(root)
            except Exception:
                pass

if __name__ == "__main__":
    if len(sys.argv) >= 3:
        sync_clean(sys.argv[1], sys.argv[2])

