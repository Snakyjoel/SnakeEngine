function onMoveCamera(focus)
    if focus == 'dad' then
        setProperty('defaultCamZoom', 1.0)
        doTweenZoom('camZoomTween', 'camGame', 1.0, 0.6, 'backInOut')
    elseif focus == 'boyfriend' then
        setProperty('defaultCamZoom', 0.8)
        doTweenZoom('camZoomTween', 'camGame', 0.8, 0.6, 'backInOut')
    end
end

local noteAnims = { 'purple', 'blue', 'green', 'red' }
local noteNames = { 'Left', 'Down', 'Up', 'Right' }

local poolSize = 8
local poolIndex = 1

function onCreatePost()
    defaultZoom = getProperty('camZoom')

    for i = 1, poolSize do
        local tag = 'noteHitPool_' .. i
        makeAnimatedLuaSprite(tag, '../shared/images/noteSkins/NOTE_assets', 160 - 37, 120 - 37)
        setObjectCamera(tag, 'bottom')
        scaleObject(tag, 1.2, 1.2)
        setProperty(tag .. '.alpha', 0)
        addLuaSprite(tag, true)
    end
end

function opponentNoteHit(id, direction, noteType, isSustain)
    if isSustain == 'true' then return end

    local dirNum = tonumber(direction)
    local animPrefix = noteAnims[dirNum + 1]
    if not animPrefix then return end

    triggerEvent('Lyrics', noteNames[dirNum + 1], '')

    local tag = 'noteHitPool_' .. poolIndex
    poolIndex = poolIndex + 1
    if poolIndex > poolSize then
        poolIndex = 1
    end

    cancelTween(tag .. '_tweenX')
    cancelTween(tag .. '_tweenY')
    cancelTween(tag .. '_tweenAlpha')

    local startX = 160 - 37
    local startY = 120 - 37
    setProperty(tag .. '.x', startX)
    setProperty(tag .. '.y', startY)
    setProperty(tag .. '.alpha', 1)

    addAnimationByPrefix(tag, 'idle', animPrefix, 24, false)
    objectPlayAnimation(tag, 'idle', true)

    local targetX = startX
    local targetY = startY
    local offset = 40

    if dirNum == 0 then
        targetX = startX - offset
    elseif dirNum == 1 then
        targetY = startY + offset
    elseif dirNum == 2 then
        targetY = startY - offset
    elseif dirNum == 3 then
        targetX = startX + offset
    end

    doTweenX(tag .. '_tweenX', tag, targetX, 0.5, 'circOut')
    doTweenY(tag .. '_tweenY', tag, targetY, 0.5, 'circOut')
    doTweenAlpha(tag .. '_tweenAlpha', tag, 0, 0.5, 'linear')
end

function onTweenCompleted(tag)
    if stringEndsWith(tag, '_tweenAlpha') then
        triggerEvent('Lyrics', '', '')
    end
end

function yey()
    triggerEvent('Lyrics', "That's how you do it!", '')
    local duration = (stepCrochet * 4) / 1000
    runTimer('yeyAnimations', duration, 1)
end

function onTimerCompleted(tag)
    if tag == 'yeyAnimations' then
        triggerEvent('Play Animation', 'cheer', 'dad')
        triggerEvent('Play Animation', 'hey', 'bf')
    end
end

local yeySteps = { 120, 184 }

function onStepHit()
    for _, step in ipairs(yeySteps) do
        if curStep == step then
            yey()
            break
        end
    end
end
