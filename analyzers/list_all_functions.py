import os, sys
import pdb, re

functionPattern = r'(\w*)\s*\(.*?\)\s*\{'
functionNamesFile = r'function_names'

allFuncList = []

def getFunctionNames(bashFileName):
	with open(bashFileName) as f:
		fileText = f.read()
	return re.findall(functionPattern, fileText)
	
def processFile( fileName ):
	funcList = getFunctionNames( fileName )
	allFuncList.extend( funcList )
	
def main():
	if len(sys.argv) != 2:
		print "invalid args"
		exit
	rootdir = sys.argv[1]
	for dirname, subDirs, files in os.walk(rootdir):
		for f in files:
			if not f.endswith(".py"):
				bashFileName = os.path.join(dirname, f)
				processFile(bashFileName)
	with open(functionNamesFile, 'w') as f:
		print 'hello'
		allFuncListUnique = list(set(allFuncList))
		allFuncListNewLines = [ (x + '\n') for x in allFuncListUnique ]
		f.writelines(allFuncListNewLines)

if __name__ == "__main__":
	#pdb.set_trace()
	main()
