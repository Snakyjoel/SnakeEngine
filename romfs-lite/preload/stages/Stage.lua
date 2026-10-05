function onCreate()
	makeLuaSpriteXML('floor', 'stages/stage', 'floor', 24, -124)
	scaleObject('floor', 3.5, 3.5)
	setScrollFactor('floor', 1.0, 1.0)

	makeLuaSpriteXML('top', 'stages/stage', 'top', 100, -459)
	scaleObject('top', 2.86, 2.86)
	setScrollFactor('top', 1.5, 1.5)

	addLuaSprite('floor', false)
	addLuaSprite('top', true)

	set3dDepth('camGame', -7.0)

	set3dDepth('top', 5.0)


	set3dDepth('camHUD', 5.0)

	set3dDepth('healthBar', 3.0)
	set3dDepth('healthBarBG', 3.0)
	set3dDepth('timeBar', 3.0)
	set3dDepth('timeBarBG', 3.0)
	set3dDepth('timeTxt', 3.0)

	set3dDepth('iconP1', 5.0)
	set3dDepth('iconP2', 5.0)
end

-- I was too lazy to edit the lua
function makeLuaSpriteXML(tag, image, spriteName, x, y)
	makeAnimatedLuaSprite(tag, image, x or 0, y or 0)
	addAnimationByPrefix(tag, spriteName, spriteName, 1, false)
	objectPlayAnimation(tag, spriteName, true)
end

local heySteps = {
	28, 60, 92, 124, 156, 188, 190, 220, 252,
	284, 348, 380, 412, 444, 446, 476, 508
}

function onStepHit()
	if songName == 'bopeebo' then
		for _, step in ipairs(heySteps) do
			if curStep == step then
				triggerEvent('Play Animation', 'hey', 'bf')
				break
			end
		end
	end
end
