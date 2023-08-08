import os, sys
import pdb

def translate(bashFileName, html):
	os.system('bash-4.3.30/bash2pyengine ' + html + bashFileName)

def helpx():
	print("Usage:\n./bash2py [-h] [-d] <dir_name>\n./bash2py [-h] [-f] <file_name>\n  -h: generate html diff not python code")

def main():

	html   = ''
	option = ''

	lth = len(sys.argv);

	if (lth == 1):
		helpx()
		exit(0)

	bashFileName = sys.argv[lth-1];
	for i in range(1,lth - 1) :
		if (sys.argv[i] == '-f' or sys.argv[i] == '-d') :
			option = sys.argv[i]
		elif (sys.argv[i] == '-h') :
			html   = ' --html '
	
	if (option == ''):
		if (os.path.isdir(bashFileName)) :
			option = '-d'
		elif (os.path.isfile(bashFileName)) :
			option = '-f'

	if (option == '-f'):
		translate(bashFileName, html)
		exit(0)

	if(option == '-d'):
		for dirname, subDirs, files in os.walk(bashFileName):
			for f in files:
				if not f.endswith(".py") and not f.endswith(".html"):
					bashFileName1 = os.path.join(dirname, f)
					translate(bashFileName1, html)
		exit(0)

if __name__ == "__main__":
	main()
