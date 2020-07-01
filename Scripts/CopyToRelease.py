import os
import shutil

copyTo = "D:\Release\VerusEdit (Data)\Data"
excludeExt = set([".py", ".7z", ".zip", ".flac", ".ini", ".png"])
excludeFiles = set([
"AllowedProps.txt",
"Foo.xxx",
"ProjectDir.txt",
"Antigul.dds",
"Antigul.mat",
"G3_Music0_SHORT.ogg",
"G3_Music1_SHORT.ogg",
"G3_Music2_SHORT.ogg",
"TODO.txt",
"Island.dds",
"Island.xml"])
excludeRoot = set(["Geometry"])
allowedActors = set(["Agent", "Blueberry", "Bulldozer", "Gulman", "Lamantina", "Soldier", "Strawberry", "VANO", "MAD"])
excludeTex = set(["Elevation", "Maps", "Splat"])

allowedProps = [line.rstrip('\n') for line in open('AllowedProps.txt')]

for root, dirs, files in os.walk(".", "*"):
	if root == ".":
		dirs[:] = [d for d in dirs if d not in excludeRoot]
	if root == ".\Models\Actors" or root == ".\Models\Motion":
		dirs[:] = [d for d in dirs if d in allowedActors]
	if root == ".\Models\Props":
		dirs[:] = [d for d in dirs if d in allowedProps]
	if root == ".\Textures":
		dirs[:] = [d for d in dirs if d not in excludeTex]
	if root != ".":
		dirTo = os.path.join(copyTo, root[2:])
		os.makedirs(dirTo)
	for file in files:
		if file in excludeFiles:
			continue
		name, ext = os.path.splitext(file)
		if ext in excludeExt:
			continue
		if name.startswith('__'):
			continue
		path = os.path.join(root, file)
		pathTo = os.path.join(copyTo, path[2:])
		print(pathTo)
		shutil.copyfile(path, pathTo)