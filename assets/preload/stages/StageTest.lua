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