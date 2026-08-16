function onCreate()
	makeLuaSprite('sky', 'stages/weekend1/sky', 247, -252);
	scaleObject('sky', 4.8, 5);
	setScrollFactor('sky', 0.1, 0.1);

	makeLuaSprite('back', 'stages/weekend1/back', 62, -192);
	scaleObject('back', 2.5, 2.5);
	setScrollFactor('back', 0.8, 0.8);

	makeLuaSprite('floor', 'stages/weekend1/floor', 49, -121);
	scaleObject('floor', 2.5, 2.5);
	setScrollFactor('floor', 1.0, 1.0);

	addLuaSprite('sky', false);
	addLuaSprite('back', false);
	addLuaSprite('floor', false);
end