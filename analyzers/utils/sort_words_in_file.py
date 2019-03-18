import sys

filename = sys.argv[1]
with open(filename) as f:
	lines = f.readlines()
lines = [x.strip() for x in lines if x.strip() != '']
lines = sorted(lines)
with open(filename, "w") as f:
	for x in lines:
		f.write("%s\n" % x)

