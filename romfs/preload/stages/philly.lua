function onCreate()
	makeLuaSpriteXML('floor', 'stages/philly/front', 'street', 0, 120)
	scaleObject('floor', 3, 3)

	makeLuaSpriteXML('behindTrain', 'stages/philly/front', 'behindTrain', -65, -122.5)
	scaleObject('behindTrain', 1.6, 1.65)

	makeLuaSpriteXML('sky', 'stages/philly/others', 'sky', -250, -200)
	scaleObject('sky', 3, 3)
	setScrollFactor('sky', 0.25, 0.25)


	local cityPos = { -265, -125 } -- x, y
	local citySize = { 2.5, 2.5 }
	local cityScrollFactor = { 0.5, 0.5 }

	makeLuaSpriteXML('city', 'stages/philly/back', 'city', cityPos[1], cityPos[2])
	scaleObject('city', citySize[1], citySize[2])
	setScrollFactor('city', cityScrollFactor[1], cityScrollFactor[2])

	makeLuaSpriteXML('window', 'stages/philly/back', 'window', cityPos[1] + 40, cityPos[2] + 20)
	scaleObject('window', citySize[1], citySize[2])
	setScrollFactor('window', cityScrollFactor[1], cityScrollFactor[2])
	setProperty('window.alpha', 0)


	addLuaSprite('sky', false)
	addLuaSprite('city', false)
	addLuaSprite('window', false)
	addLuaSprite('behindTrain', false)
	addLuaSprite('floor', false)
end

-- I was too lazy to edit the lua (SnakyJoel words 🙏🥀)
function makeLuaSpriteXML(tag, image, spriteName, x, y)
	makeAnimatedLuaSprite(tag, image, x or 0, y or 0)
	addAnimationByPrefix(tag, spriteName, spriteName, 1, false)
	objectPlayAnimation(tag, spriteName, true)
end

-- Lista de los colores originales de Philly
local phillyLightsColors = { '31A2FD', '31FD8C', 'FB33F5', 'FD4531', 'FBA633' }

function onBeatHit()
	-- Cambia de color cada 4 beats (1 compás)
	if curBeat % 4 == 0 and curBeat > 4 then
		-- Guarda el color random en una variable
		local windowColor = phillyLightsColors[getRandomInt(1, 5)]
		-- Hace que la ventana sea visible
		setProperty('window.alpha', 1)
		-- Cambia el color de la ventana
		setProperty('window.color', windowColor)
		-- Hace que la ventana sea semi-transparente
		doTweenAlpha('windowAlphaTween', 'window', 0, 1.75, 'linear')
	end
end
