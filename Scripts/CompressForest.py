import glob, os

exe = "C:\\Home\\Projects\\Verus\\verus\\Bin\\TextureTool.exe"

for file in glob.glob("*.GB0.psd"):
	os.system("\"" + exe + "\" " + file)

for file in glob.glob("*.GB1.FX.psd"):
	os.system("\"" + exe + "\" --non-color-data " + file)

for file in glob.glob("*.GB2.FX.psd"):
	os.system("\"" + exe + "\" --non-color-data " + file)
