import operator
import matplotlib.pyplot as plt
import pprint
import json
import os, sys, re
import numpy
#from path import path
#commandsListFileName = 'commands'
statsFileName = 'stats'
dataFileName = 'data'

pattern1 = r'FILE \#(\d+)'
pattern2 = r'NUM_LINES: (\d+)'

class Summary:
	def __init__():
		totalFiles = 0
		totalLines = 0
		cmdCount = 0
		

def getData():
	'''Read data file into a dictionary'''
	fileDict = []
	cmdDict = {}
	numLines = 0
	i = 0
	#print "hello"
	with open(dataFileName) as f:
		lines = f.readlines()
		for line in lines:
			line = line.strip()
			match1 = re.match(pattern1, line)
			match2 = re.match(pattern2, line)
			if match1 != None:
				#print "abc"
				
				if i > 0:
					fileDict.append((cmdDict, numLines))
				cmdDict = {}
				numLines = 0
				i += 1
			elif match2 != None:
				#print "def"
				numLines = int(match2.group(1))
			else:
				arr = line.split()
				cmd = arr[0]
				num = int(arr[1])
				if cmd in cmdDict:
					cmdDict[cmd] += num
				else:
					cmdDict[cmd] = num
	fileDict.append((cmdDict, numLines))
	return fileDict

def amalgamateData(fileDict):
	''' Iterate through file mapping and return summary of data'''
	''' Input: list which contains mapping between each file and a tuple T, 
		where T[0] is a mapping between command and number of times appeared, and T[1] 
		is the number of lines in the file '''
	''' Output: a summary data struct '''
	lineCounts = []
	cmdMapping = {}
	numFiles = len(fileDict)
	summary = {}
	for i in range(0, numFiles):
		tup = fileDict[i]
		cmdDict = tup[0]
		numLines = tup[1]
		lineCounts.append(numLines)
		for cmd, count in cmdDict.iteritems():
			if cmd in cmdMapping:
				cmdMapping[cmd] += count
			else:
				cmdMapping[cmd] = count
	
	summary['num_files'] = numFiles
	summary['total_lines'] = numpy.sum(lineCounts)
	summary['min_lines'] = min(lineCounts)
	summary['max_lines'] = max(lineCounts)
	summary['average_num_of_lines'] = numpy.average(lineCounts)
	summary['median_lines'] = numpy.median(lineCounts)
	
	plt.boxplot(lineCounts)
	plt.ylabel("Number of lines per file")
	plt.show()
	
	plt.hist(lineCounts, bins=100)
	plt.xlabel("Number of lines per file")
	plt.ylabel("Count")
	plt.show()
	
	stdDev = numpy.std(lineCounts)
	summary['std_dev_line_counts'] = stdDev
	
	summary['cmd_mapping'] = cmdMapping
	
	sortedCmdMapping = sorted(cmdMapping.iteritems(), key=operator.itemgetter(1), reverse=True)
	sortedKeys = [x for (x, y) in sortedCmdMapping]
	sortedVals = [y for (x, y) in sortedCmdMapping]
	#print(sortedKeys)
	#print(sortedVals)
	#for (cmd, count) in sortedCmdMapping:
	#	print '%s\t%s' % (cmd, count)
	
	#plt.bar(range(len(sortedVals)), sortedVals, align='center')
	#plt.xticks(range(len(sortedKeys)), sortedKeys)
	#plt.show()
	#plt.bar(
	
	
	
	return summary
		
		

def testGetData():
	a = getData()
	print a

def test():
	testGetData()
	exit(0)

def amalgamateDict(cmdDict):
	global totalCmdDict
	for cmd, count in cmdDict.iter_items():
		if key in totalCmdDict:
			totalCmdDict[cmd] += count
		else:
			totalCmdDict[cmd] = count

def getCommandsList():
	with open(commandsListFileName) as f:
		commandsList = [x.strip() for x in f.readlines()]
	return commandsList
	

def main():
	#test()
	fileMap = getData()
	summary = amalgamateData(fileMap)
	pprint.pprint(summary)
	#pretty(summary)
	#print(json.dumps(summary))
	#print 'hello'
	#writeInfoToStatsFile(totalLines, numFiles, cmdDict)

if __name__ == '__main__':
	#print "helo"
	main()

