function onCreate()
	makeLuaSprite('sky', '', -1500, -1000)
	makeGraphic('sky', 500, 300, 'F8D45B')
	scaleObject('sky', 10, 10)
	setScrollFactor('sky', 0, 0)
	addLuaSprite('sky', false)

	makeLuaSpriteXML('mountains', 'stages/tank/fmr', 'mountains', -850, -350)
	scaleObject('mountains', 2.5, 2.5)
	setScrollFactor('mountains', 0.2, 0.2)
	addLuaSprite('mountains', false);

	makeLuaSpriteXML('clouds', 'stages/tank/cbtmlr', 'clouds', -850, -400)
	scaleObject('clouds', 2.5, 2.5)
	setScrollFactor('clouds', 0.3, 0.3)
	addLuaSprite('clouds', false);

	makeLuaSpriteXML('buildings', 'stages/tank/cbtmlr', 'buildings', -550, -130)
	scaleObject('buildings', 2, 2)
	setScrollFactor('buildings', 0.35, 0.35)
	addLuaSprite('buildings', false);

	makeLuaSpriteXML('ruins', 'stages/tank/fmr', 'ruins', -650, -250)
	scaleObject('ruins', 2.5, 2.5)
	setScrollFactor('ruins', 0.25, 0.25)
	addLuaSprite('ruins', false);

	makeAnimatedLuaSprite('SmokeBlurLeft', 'stages/tank/cbtmlr', -850, -200);
	addAnimationByPrefix('SmokeBlurLeft', 'idle', 'SmokeBlurLeft instance ', 24, true);
	setScrollFactor('SmokeBlurLeft', 0.3, 0.3);
	scaleObject('SmokeBlurLeft', 3, 3);
	addLuaSprite('SmokeBlurLeft', false);

	makeAnimatedLuaSprite('SmokeBlurRight', 'stages/tank/cbtmlr', 500, -200);
	addAnimationByPrefix('SmokeBlurRight', 'idle', 'SmokeRight instance ', 24, true);
	setScrollFactor('SmokeBlurRight', 0.3, 0.3);
	scaleObject('SmokeBlurRight', 3, 3);
	addLuaSprite('SmokeBlurRight', false);

	makeAnimatedLuaSprite('watchtower', 'stages/tank/fw', -850, -200);
	addAnimationByPrefix('watchtower', 'idle', 'watchtower gradient color instance ', 24, false);
	setScrollFactor('watchtower', 0.8, 0.8);
	scaleObject('watchtower', 3, 3);
	addLuaSprite('watchtower', false);

	makeAnimatedLuaSprite('watchtower', 'stages/tank/fw', -850, -200);
	addAnimationByPrefix('watchtower', 'idle', 'watchtower gradient color instance ', 24, false);
	setScrollFactor('watchtower', 0.8, 0.8);
	scaleObject('watchtower', 3, 3);
	addLuaSprite('watchtower', false);

	makeLuaSpriteXML('floor', 'stages/tank/fmr', 'floor', -700, -440)
	scaleObject('floor', 2.8, 2.8)
	setScrollFactor('floor', 1.0, 1.0)
	addLuaSprite('floor', false);

	makeLuaSpriteXML('bricksGround', 'stages/tank/fmr', 'bricksGround', -400, 180)
	scaleObject('bricksGround', 1.3, 1.3)
	setScrollFactor('bricksGround', 1.0, 1.0)
	addLuaSprite('bricksGround', true);

	makeAnimatedLuaSprite('tankheadside', 'stages/tank/fw', -1350, 170);
	addAnimationByPrefix('tankheadside', 'idle', 'fg tankhead far right instance ', 24, false);
	setScrollFactor('tankheadside', 1.2, 1.2);
	scaleObject('tankheadside', 2.2, 2.2);
	addLuaSprite('tankheadside', true);

	makeAnimatedLuaSprite('tankheadside2', 'stages/tank/fw', 450, 170);
	addAnimationByPrefix('tankheadside2', 'idle', 'fg tankhead far right instance ', 24, false);
	setScrollFactor('tankheadside2', 1.2, 1.2);
	setProperty('tankheadside2.flipX', true)
	scaleObject('tankheadside2', 2.2, 2.2);
	addLuaSprite('tankheadside2', true);

	makeAnimatedLuaSprite('tankheadside3', 'stages/tank/fw', 150, 370);
	addAnimationByPrefix('tankheadside3', 'idle', 'fg tankhead far right instance ', 24, false);
	setScrollFactor('tankheadside3', 1.2, 1.2);
	setProperty('tankheadside3.flipX', true)
	scaleObject('tankheadside3', 2.2, 2.2);
	addLuaSprite('tankheadside3', true);

	makeAnimatedLuaSprite('foregroundman', 'stages/tank/fw', -380, 500);
	addAnimationByPrefix('foregroundman', 'idle', 'foreground man 3 instance ', 24, false);
	setScrollFactor('foregroundman', 1.5, 1.5);
	scaleObject('foregroundman', 2, 2);
	addLuaSprite('foregroundman', true);

	makeAnimatedLuaSprite('tankheadbiglmao', 'stages/tank/fw', -1050, 500);
	addAnimationByPrefix('tankheadbiglmao', 'idle', 'fg tankhead 4 instance ', 24, false);
	setScrollFactor('tankheadbiglmao', 1.3, 1.3);
	scaleObject('tankheadbiglmao', 2, 1.8);
	addLuaSprite('tankheadbiglmao', true);

	makeAnimatedLuaSprite('tankheadbiglmao2', 'stages/tank/fw', -200, 450);
	addAnimationByPrefix('tankheadbiglmao2', 'idle', 'fg tankhead 4 instance ', 24, false);
	setScrollFactor('tankheadbiglmao2', 2, 2);
	scaleObject('tankheadbiglmao2', 2.1, 2.1);
	setProperty('tankheadbiglmao2.flipX', true)
	addLuaSprite('tankheadbiglmao2', true);
end

function onBeatHit()
	objectPlayAnimation('watchtower', 'idle', true)
	objectPlayAnimation('tankheadside', 'idle', true)
	objectPlayAnimation('tankheadbiglmao', 'idle', true)
	objectPlayAnimation('foregroundman', 'idle', true)
	objectPlayAnimation('tankheadbiglmao2', 'idle', true)
	objectPlayAnimation('tankheadside2', 'idle', true)
	objectPlayAnimation('tankheadside3', 'idle', true)
end

function onStepHit()
	if songName == 'ugh' then
		if curStep == 60 then
			playAnimFES('dad', 'shared/images/characters/extraAnims/tankmanUgh', 'TANKMAN UGH ', 24, false, 0, 0)
		end
	end
end

-- I was too lazy to edit the lua
function makeLuaSpriteXML(tag, image, spriteName, x, y)
	makeAnimatedLuaSprite(tag, image, x or 0, y or 0)
	addAnimationByPrefix(tag, spriteName, spriteName, 1, false)
	objectPlayAnimation(tag, spriteName, true)
end
