import sys

def main():
    cacheHeader = ">>>>>CACHE_OUTPUT<<<<<\n"
    if(len(sys.argv) < 3):
        print('ERROR: too few arguments. Usage: "python3 interpretcache.py <<output_csv_file>> <<input_files>>"')
    csvDict = {}
    for i in range(2, len(sys.argv)):
        with open(sys.argv[i], 'r') as inputFile:
            csvSection = False
            coreID = -1
            for line in inputFile:
                if(line[0:2] == "I$"):
                    coreID += 1
                if(csvSection and line[0:2] != "I$" and line[0:2] != "D$"):
                    splitName = sys.argv[i].split('-')
                    partitionScheme = int(splitName[1])
                    cacheSize = int(splitName[2])
                    if(partitionScheme not in csvDict.keys()):
                        csvDict[partitionScheme] = {}
                    if(cacheSize not in csvDict[partitionScheme].keys()):
                        csvDict[partitionScheme][cacheSize] = ""
                    csvDict[partitionScheme][cacheSize] += str(partitionScheme) + ", " + str(cacheSize) + ", " + str(coreID) + ", " + line
                if(line == cacheHeader):
                    csvSection = True
    with open(sys.argv[1], 'w') as outputFile:
        outputFile.write("partition scheme, cache sets, core id, cache name, bytes read, bytes written, read accesses, write accesses, read misses, write misses, writebacks, miss rate, read misses in LLC, write misses in LLC, total miss rate\n")
        for partitionScheme in sorted(csvDict.keys()):
            for cacheSize in sorted(csvDict[partitionScheme].keys()):
                csvContent = csvDict[partitionScheme][cacheSize]
                if(partitionScheme != 0):
                    #outputFile.write(csvContent)
                    None
                csvLines = csvContent.split('\n')
                bytesRead = 0
                bytesWritten = 0
                readAccesses = 0
                writeAccesses = 0
                readMisses = 0
                writeMisses = 0
                writebacks = 0
                llcReadMisses = 0
                llcWriteMisses = 0
                for line in csvLines:
                    if(line == ""):
                        break
                    values = line.split(", ")
                    bytesRead += int(values[4])
                    bytesWritten += int(values[5])
                    readAccesses += int(values[6])
                    writeAccesses += int(values[7])
                    readMisses += int(values[8])
                    writeMisses += int(values[9])
                    writebacks += int(values[10])
                    if(partitionScheme == 1):
                        llcReadMisses += int(values[12])
                        llcWriteMisses += int(values[13])
                missRate = 1.0*(readMisses+writeMisses)/(readAccesses+writeAccesses)
                totalLine = str(partitionScheme) + ", " + \
                    str(cacheSize) + ", NaN, Total, " + \
                    str(bytesRead) + ", " + \
                    str(bytesWritten) + ", " + \
                    str(readAccesses) + ", " + \
                    str(writeAccesses) + ", " + \
                    str(readMisses) + ", " + \
                    str(writeMisses) + ", " + \
                    str(writebacks) + ", " + \
                    str(missRate) + ", "
                if partitionScheme == 1:
                    totalLine += str(llcReadMisses) + ", " + \
                    str(llcWriteMisses) + ", " + \
                    str(1.0*(readMisses+writeMisses+llcReadMisses+llcWriteMisses)/(readAccesses+writeAccesses))
                else:
                    totalLine += "0, 0, " + str(missRate)
                totalLine += '\n'
                outputFile.write(totalLine)


main()
