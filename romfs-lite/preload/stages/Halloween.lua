function onCreate()
	makeAnimatedLuaSprite('bg', 'stages/halloween/halloween_bg', -650, -550);
	addAnimationByPrefix('bg', 'idle', 'halloweem bg', 1, true);
	addAnimationByPrefix('bg', 'light', 'halloweem bg lightning strike', 8, false);
	objectPlayAnimation('bg', 'idle', false);
	setScrollFactor('bg', 1, 1);
	scaleObject('bg', 6.8, 6.8);

	addLuaSprite('bg', false);
end

local strikeDuration = 0

function onUpdate(elapsed)
	if strikeDuration > 0 then
		strikeDuration = strikeDuration - elapsed
		if strikeDuration <= 0 then
			objectPlayAnimation('bg', 'idle', true)
		end
	end
end

function bgLight()
	objectPlayAnimation('bg', 'light', true)
	playAnimFES('gf', 'shared/images/characters/extraAnims/gf_scared', 'GF FEAR', 8, false, -2, -5)
	playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bf_scared', 'BF idle shaking', 8, false, 0, 0)
	strikeDuration = 1.0
end

function onStepHit()
	if songName == 'spookeez' then
		if curStep == 16 then
			bgLight()
		end
		if curStep == 576 then
			bgLight()
		end
		if curStep == 640 then
			bgLight()
		end
		if curStep == 704 then
			bgLight()
		end
		if curStep == 768 then
			bgLight()
		end
		if curStep == 832 then
			bgLight()
		end
		if curStep == 896 then
			bgLight()
		end
	end
end