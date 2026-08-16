local limoDancerCopies = 3 -
1                              -- segun el original hay 5 (EL -1 NO SE TOCA, ES PARA QUE LOS OFFSETS EN EL BUCLE FUNCIONEN BIEN)

function onCreate()
	makeLuaSprite('back', 'stages/mom/back', -250, 0)
	scaleObject('back', 2.3, 2.3)
	setScrollFactor('back', 0.25, 0.25)

	makeAnimatedLuaSprite('limoDrive', 'stages/mom/limoDrive', 0, 400)
	scaleObject('limoDrive', 2.3, 2.3)
	addAnimationByPrefix('limoDrive', 'Limo stage', 'Limo stage', 24, true)
	playAnim('limoDrive', 'Limo stage', true)

	makeAnimatedLuaSprite('bgLimo', 'stages/mom/bgLimo', 0, 250)
	scaleObject('bgLimo', 2.3, 2.3)
	addAnimationByPrefix('bgLimo', 'background limo pink', 'background limo pink', 24, true)
	playAnim('bgLimo', 'background limo pink', true)
	setScrollFactor('bgLimo', 0.35, 0.35)

	makeLuaSprite('car', 'stages/mom/fastCarLol', -2000, 100)
	scaleObject('car', 1, 1)

	local limoDancerBaseX = -100
	local limoDancerXOffset = 350

	for i = 0, limoDancerCopies do
		makeAnimatedLuaSprite('limoDancer' .. i, 'stages/mom/limoDancer', limoDancerBaseX + limoDancerXOffset * i, -175)
		scaleObject('limoDancer' .. i, 1.75, 1.75)
		addAnimationByPrefix('limoDancer' .. i, 'bg dancer sketch PINKL', 'bg dancer sketch PINKL', 24, false)
		addAnimationByPrefix('limoDancer' .. i, 'bg dancer sketch PINKR', 'bg dancer sketch PINKR', 24, false)
		setScrollFactor('limoDancer' .. i, 0.35, 0.35)
	end


	--addOffset('limoDancer', 'bg dancer sketch PINKR', -1500, 0)


	addLuaSprite('back', false)
	addLuaSprite('bgLimo', false)

	for i = 0, limoDancerCopies do
		addLuaSprite('limoDancer' .. i, false)
	end

	addLuaSprite('limoDrive', false)
	addLuaSprite('car', true)
	setProperty('gf.visible', false)
end

-- I was too lazy to edit the lua (SnakyJoel words 🙏🥀)
function makeLuaSpriteXML(tag, image, spriteName, x, y)
	makeAnimatedLuaSprite(tag, image, x or 0, y or 0)
	addAnimationByPrefix(tag, spriteName, spriteName, 1, false)
	objectPlayAnimation(tag, spriteName, true)
end

local nextCar = math.random(8, 12)

function onBeatHit()
	if curBeat == nextCar then
		setProperty('car.x', -2000)
		playSound('carPass', 0.5)
		doTweenX('car', 'car', 2000, 0.5, 'linear')
		nextCar = curBeat + math.random(6, 12)
	end

	for i = 0, limoDancerCopies do
		if curBeat > 0 then
			if curBeat % 2 ~= 0 then
				playAnim('limoDancer' .. i, 'bg dancer sketch PINKR', true)
				setProperty('limoDancer' .. i .. '.x', (getProperty('limoDancer' .. i .. '.x') - 410))
				setProperty('limoDancer' .. i .. '.y', -162.5)
			elseif curBeat % 2 == 0 then
				playAnim('limoDancer' .. i, 'bg dancer sketch PINKL', true)
				setProperty('limoDancer' .. i .. '.x', (getProperty('limoDancer' .. i .. '.x') + 410))
				setProperty('limoDancer' .. i .. '.y', -175)
			end
		end
	end
end
