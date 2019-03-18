import os, sys
import pdb
commandsListFileName = 'commands'
dataFileName = 'data'

def getDataOnBashFile(bashFileName):
	"""Process bash file and return tuple with numLines, and dictionary containing mapping of construct to count"""
	with open(bashFileName) as f:
		bashLines = f.readlines()
		bashText = ''.join(bashLines)
	numLines = len(bashLines)
	cmdDict = {}
	#pdb.set_trace()
	for cmd in commandsList:
		cmdDict[cmd] = bashText.count(cmd)
	return (numLines, cmdDict)

def processBashFile(bashFileName, whichFile):
	"""First get data on bash file, then write it to data file"""
	infoTuple = getDataOnBashFile(bashFileName)
	numLines = infoTuple[0]
	cmdDict = infoTuple[1]
	writeInfoToDataFile(numLines, cmdDict, whichFile)

def writeInfoToDataFile(numLines, cmdDict, whichFile):
	"""infoTuple has numLines, and dictionary of mapping of constructs to counts"""
	global firstTime
	if firstTime:
		openOption = 'w'
		firstTime = False
	else:
		openOption = 'a'
	dataFile = open(dataFileName, openOption)
	dataFile.write("FILE #" + str(whichFile) + "\n")
	dataFile.write("NUM_LINES: " + str(numLines) + "\n")
	for cmd, count in cmdDict.iteritems():
		dataFile.write(cmd + " " + str(count) + "\n")
	dataFile.close()

def getCommandsList():
	with open(commandsListFileName) as f:
		commandsList = [x.strip() for x in f.readlines()]
	return commandsList




def helpx():
	print "Usage:\nTo analyze a single file: bdc -f filename\nTo recursively analyze all files in a directory: bdc -d dirname"
	exit(1)


	

commandsList = getCommandsList()
firstTime = True
def main():
	if (len(sys.argv) != 3):
		helpx()
	option = sys.argv[1]
	if (option == '-f'):
		bashFileName = sys.argv[2]
		processBashFile(bashFileName, 1)
	elif(option == '-d'):
		rootdir = sys.argv[2]
		whichFile = 1
		for dirname, subDirs, files in os.walk(rootdir):
			for f in files:
				bashFileName = os.path.join(dirname, f)
				processBashFile(bashFileName, whichFile)
				whichFile += 1
	else:
		helpx()
	#writeInfoToStatsFile(totalLines, numFiles, cmdDict)

if __name__ == "__main__":
	main()

