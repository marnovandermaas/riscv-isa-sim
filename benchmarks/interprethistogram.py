import sys
#import matplotlib.pyplot as plt

def main():
  histogramHeader = ">>>>>PC_HISTORGRAM<<<<<\n"
  csvDicts = []
  if(len(sys.argv) < 3):
    print('ERROR: too few arguments, please use by running "python3 producehistogramcsv.py <<input_file>> <<output_file>>"')
    return
  #print("Intput file: " + sys.argv[1])
  csvSection = False;
  with open(sys.argv[1], 'r') as inputFile:
    for line in inputFile:
      if(csvSection):
        splitLine = line[:-1].split(', ')
        if(splitLine[0] == 'Size'):
          #print(line, end='')
          csvDicts.append({})
        else:
          address = int(splitLine[0], 16)
          count = int(splitLine[1])
          if(address > 0x80000000):
            address &= 0xFFF00FFF
          csvDicts[-1][address] = count
      if(line == histogramHeader):
        csvSection = True
  with open(sys.argv[2], 'w') as outputFile:
    combinedDict = {}
    for (idx, dictionary) in enumerate(csvDicts):
      for key in dictionary.keys():
        if key not in combinedDict.keys():
          combinedDict[key] = []
        while len(combinedDict[key]) < idx:
          combinedDict[key].append(0)
        combinedDict[key].append(dictionary[key])
    for key in sorted(combinedDict.keys()):
      csvLine = ""
      while len(combinedDict[key]) < len(csvDicts):
        combinedDict[key].append(0)
      csvLine += hex(key) + ", "
      total = 0
      for item in combinedDict[key]:
        total += item
        csvLine += str(item) + ", "
      csvLine += str(total) + "\n"
      #print(csvLine)
      outputFile.write(csvLine)


main()
