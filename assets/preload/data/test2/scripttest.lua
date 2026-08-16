function onCreate()
    debugPrint("TEST DE LUA")

    makeAnimatedLuaSprite('testAnim', 'rings', 500, 100)
    addAnimationByPrefix('testAnim', 'idle', 'rings idle', 24, true)
    setProperty('testAnim.alpha', 0.0)
    scaleObject('testAnim', 5, 5)
    addLuaSprite('testAnim', true)

    makeLuaText('hudText', 'Esperando...', 0, 10, 20)
    setTextSize('hudText', 12)
    addLuaText('hudText', true)
    
    makeLuaText('worldText', 'TETAS', 0, 0, 0)
    setTextColor('worldText', '00FF00')
    setObjectCamera('worldText', 'camGame')
    setProperty('worldText.alpha', 0.0)
    addLuaText('worldText', true)
end

function onUpdate(elapsed)
    setTextString('hudText', 'Beat: ' .. curBeat .. ' | Score: ' .. score .. ' | Health: ' .. string.format("%.2f", getHealth()))
end

function onBeatHit()
    if curBeat == 4 then
        scaleObject('testAnim', 6, 6)
    end

    if curBeat == 8 then
        setProperty('testAnim.alpha', 0.5)
    end

    if curBeat == 12 then
        setProperty('worldText.alpha', 0.7)
    end

    if curBeat == 16 then
        setProperty('defaultCamZoom', 1.2)
        addHealth(0.5)
        cameraShake('hud', 0.02, 0.5)
    end

    if curBeat == 20 then
        doTweenX('boyfriendtween', 'boyfriend', 100, 2.0, 'cubeInOut')
        doTweenAlpha('chauBg', 'testAnim', 1, 2, 'linear')
    end

    if curBeat == 24 then
        characterDance('dad')
    end

    if curBeat == 28 then
        setProperty('defaultCamZoom', 0.7)
    end
end