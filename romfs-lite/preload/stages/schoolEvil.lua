local bgpath = 'stages/school/schoolEVIL'

function onCreate()
    makeAnimatedLuaSprite('school', bgpath, 450, 524)
    addAnimationByPrefix('school', 'school', 'school', 0, false)
    setScrollFactor('school', 0.6, 0.9)
    setProperty('school.antialiasing', false)
    scaleObject('school', 6, 6)
    addLuaSprite('school', false)

    makeAnimatedLuaSprite('street', bgpath, 750, 660)
    addAnimationByPrefix('street', 'street', 'floor', 0, false)
    setScrollFactor('street', 1.0, 1.0);
    setProperty('street.antialiasing', false);
    scaleObject('street', 6, 6);
    addLuaSprite('street', false);

    setCameraShader("camGame", "wave", 2.0, 2.0, 2.0)

    set3dDepth('camGame', -7.0)

    set3dDepth('school', -7.0)

    set3dDepth('camHUD', 5.0)

    set3dDepth('healthBar', 3.0)
    set3dDepth('healthBarBG', 3.0)
    set3dDepth('timeBar', 3.0)
    set3dDepth('timeBarBG', 3.0)
    set3dDepth('timeTxt', 3.0)

    set3dDepth('iconP1', 5.0)
    set3dDepth('iconP2', 5.0)
end
