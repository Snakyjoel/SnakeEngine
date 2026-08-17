local bgpath = 'stages/school/school'
hasCreeps = false

function onCreate()
    if dadName == 'senpai-angry' then
        hasCreeps = true
    else
        hasCreeps = false
    end
    makeAnimatedLuaSprite('sky', bgpath, 0, -150)
    addAnimationByPrefix('sky', 'sky', 'weebSky', 0, false)
    setScrollFactor('sky', 0.1, 0.1)
    setProperty('sky.antialiasing', false)
    scaleObject('sky', 6, 6)
    addLuaSprite('sky', false)

    makeAnimatedLuaSprite('school', bgpath, 390, 524) -- times 6?
    addAnimationByPrefix('school', 'school', 'weebSchool', 0, false)
    setScrollFactor('school', 0.6, 0.9)
    setProperty('school.antialiasing', false)
    scaleObject('school', 6, 6)
    addLuaSprite('school', false)

    makeAnimatedLuaSprite('street', bgpath, 750, 960)
    addAnimationByPrefix('street', 'street', 'weebStreet', 0, false)
    setScrollFactor('street', 0.95, 0.95);
    setProperty('street.antialiasing', false);
    scaleObject('street', 6, 6);
    addLuaSprite('street', false);

    local trees1X = 240
    local trees2X = 700
    local treesY = 390
    local treesfps = 5
    if not lowQuality then
        -- trees back
        makeAnimatedLuaSprite('weebTree1Back', bgpath, trees1X - 70,
            treesY + 220)
        addAnimationByPrefix('weebTree1Back', 'weebTree1Back', 'weebTree1Back',
            0, false)
        setScrollFactor('weebTree1Back', 0.85, 0.85)
        setProperty('weebTree1Back.antialiasing', false)
        scaleObject('weebTree1Back', 6, 6)
        addLuaSprite('weebTree1Back', false)

        makeAnimatedLuaSprite('weebTree2Back', bgpath, 1380,
            treesY + 220)
        addAnimationByPrefix('weebTree2Back', 'weebTree2Back', 'weebTree2Back',
            0, false)
        setScrollFactor('weebTree2Back', 0.85, 0.85)
        setProperty('weebTree2Back.antialiasing', false)
        scaleObject('weebTree2Back', 6, 6)
        addLuaSprite('weebTree2Back', false)
    end

    makeAnimatedLuaSprite('trees1', bgpath, trees1X, treesY)
    addAnimationByPrefix('trees1', 'trees1', 'weebTrees_', treesfps, true)
    setScrollFactor('trees1', 0.85, 0.85)
    setProperty('trees1.antialiasing', false)
    scaleObject('trees1', 6, 6)
    addLuaSprite('trees1', false)

    makeAnimatedLuaSprite('trees2', bgpath, trees2X, treesY)
    addAnimationByPrefix('trees2', 'trees2', 'weebTrees_', treesfps, true)
    setScrollFactor('trees2', 0.85, 0.85)
    setProperty('trees2.antialiasing', false)
    setProperty('trees2.flipX', true)
    scaleObject('trees2', 6, 6)
    addLuaSprite('trees2', false)

    if not lowQuality then
        -- petals
        -- girls
        local girlsframerate = 8
        local girlsspacing = 500
        local girlsY = 590
        local girlsXoffset = -700
        for i = 1, 3 do
            makeAnimatedLuaSprite('girl1-' .. i, bgpath,
                430 + girlsXoffset + girlsspacing * i, girlsY)
            addAnimationByPrefix('girl1-' .. i, 'danceLeft', 'girl1idleleft',
                girlsframerate, false)
            addAnimationByPrefix('girl1-' .. i, 'danceRight', 'girl1idleright',
                girlsframerate, false)
            addAnimationByPrefix('girl1-' .. i, 'idle', 'girl1loveidle',
                girlsframerate - 1, false)
            setScrollFactor('girl1-' .. i, 0.9, 0.9)
            setProperty('girl1-' .. i .. '.antialiasing', false)
            scaleObject('girl1-' .. i, 6, 6)
            addLuaSprite('girl1-' .. i, false)

            makeAnimatedLuaSprite('girl2-' .. i, bgpath,
                680 + girlsXoffset + girlsspacing * i, girlsY)
            addAnimationByPrefix('girl2-' .. i, 'danceLeft', 'girl2idleleft',
                girlsframerate, false)
            addAnimationByPrefix('girl2-' .. i, 'danceRight', 'girl2idleright',
                girlsframerate, false)
            addAnimationByPrefix('girl2-' .. i, 'idle', 'girl2loveidle',
                girlsframerate - 1, false)
            setScrollFactor('girl2-' .. i, 0.9, 0.9)
            setProperty('girl2-' .. i .. '.antialiasing', false)
            scaleObject('girl2-' .. i, 6, 6)
            addLuaSprite('girl2-' .. i, false)

            if hasCreeps then
                playAnim('girl1-' .. i, 'danceLeft', true)
                playAnim('girl2-' .. i, 'danceLeft', true)
            else
                playAnim('girl1-' .. i, 'idle', true)
                playAnim('girl2-' .. i, 'idle', true)
            end
        end
    end
end

function onBeatHit()
    if hasCreeps then
        creepsDance()
    else
        loveDance()
    end
    --
end

danceDir = true
function creepsDance()
    danceDir = not danceDir;
    if danceDir then
        for i = 1, 3 do
            playAnim('girl1-' .. i, 'danceRight', true);
            playAnim('girl2-' .. i, 'danceRight', true);
        end
    else
        for i = 1, 3 do
            playAnim('girl1-' .. i, 'danceLeft', true);
            playAnim('girl2-' .. i, 'danceLeft', true);
        end
    end
end

function loveDance()
    danceDir = not danceDir
    if danceDir then
        for i = 1, 3 do
            playAnim('girl1-' .. i, 'idle', true);
        end
    else
        for i = 1, 3 do
            playAnim('girl2-' .. i, 'idle', true);
        end
    end
end
