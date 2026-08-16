function onCreate()
	makeLuaSpriteXML('floor', 'stages/stage', 'floor', 24, -124)
	scaleObject('floor', 3.5, 3.5)
	setScrollFactor('floor', 1.0, 1.0)

	makeLuaSpriteXML('top', 'stages/stage', 'top', 100, -459)
	scaleObject('top', 2.86, 2.86)
	setScrollFactor('top', 1.5, 1.5)

	addLuaSprite('floor', false)
	addLuaSprite('top', true)
end

-- I was too lazy to edit the lua
function makeLuaSpriteXML(tag, image, spriteName, x, y)
	makeAnimatedLuaSprite(tag, image, x or 0, y or 0)
	addAnimationByPrefix(tag, spriteName, spriteName, 1, false)
	objectPlayAnimation(tag, spriteName, true)
end

function onStepHit()
	if songName == 'bopeebo' then
		if curStep == 28 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 60 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 92 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 124 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 156 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 188 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 190 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 220 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 252 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 284 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 348 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 380 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 412 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 444 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 446 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 476 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
		if curStep == 508 then
			playAnimFES('boyfriend', 'shared/images/characters/extraAnims/bfHey', 'BF HEY!!', 24, false, 0, 0)
		end
	end
end
