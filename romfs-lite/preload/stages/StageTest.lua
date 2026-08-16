function onCreate()
	makeLuaSprite('floor', 'stages/stage/floor', -1172.5, -791.25);
	scaleObject('floor', 5.7, 5.7);
	setScrollFactor('floor', 1.0, 1.0);

	makeLuaSprite('top', 'stages/stage/top', -1168.75, -888.75);
	scaleObject('top', 5.7, 5.7);
	setScrollFactor('top', 0.5, 0.5);

	addLuaSprite('floor', false);
	addLuaSprite('top', true);
end
--idk