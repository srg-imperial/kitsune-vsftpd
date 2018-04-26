#!/usr/bin/python
# code by michail denchev <mdenchev@gmail.com>
# python-matplotlib and livetex are required

import time
import os
import signal
import sys
import Queue
import thread

import json
import numpy

from subprocess import *
from pylab import *


# These paths are set in bench.json
driver = ""
modified_root = ""
original_root = ""
program_versions = []
bench_directory = ""
results_directory = ""
bench_script = ""
concurrent_script = "" 
latex_terse = ""
latex_verbose = ""
number_of_runs = 0
number_of_connections = 0


# Load paths and options from bench.json
def loadConf():
	global driver
	global modified_root
	global original_root
	global program_versions
	global bench_directory
	global results_directory
	global bench_script
	global concurrent_script
	global latex_terse
	global latex_verbose
	global number_of_runs
	global number_of_connections

	conf_f = open("kitsune_bench.json")
	conf = json.load(conf_f)
	conf_f.close()

	driver = conf["KitsuneSettings"]["Driver"]
	
	modified_root = conf["ProgramSettings"]["ModifiedRoot"]
	original_root= conf["ProgramSettings"]["OriginalRoot"]
	program_versions = conf["ProgramSettings"]["Versions"]
	bench_directory	= conf["BenchSettings"]["BenchDirectory"]
	results_directory = conf["BenchSettings"]["ResultsDirectory"]
	bench_script = conf["BenchSettings"]["BenchScript"]
	concurrent_script = conf["BenchSettings"]["ConcurrentScript"]
	number_of_runs = conf["BenchSettings"]["NumberOfSerialRuns"]
	number_of_connections = conf["BenchSettings"]["NumberOfConcurrentConnections"]
	
	latex_terse	= conf["OutputSettings"]["LatexTerse"]
	latex_verbose = conf["OutputSettings"]["LatexVerbose"]
		
	return conf

"""
Functions for writing to a specific latex files.
"""
def latex_init():
	global latex_terse
	global latex_verbose
	latex_terse = open("results/latex_terse.tex", "w")
	latex_verbose = open("results/latex_verbose.tex", "w")

def latex_both(text):	
	global latex_terse
	global latex_verbose
	latex_terse.write(text)
	latex_verbose.write(text)

def latex_terse(text):
	global latex_terse
	latex_terse.write(text)

def latex_verbose(text):
	global latex_verbose
	latex_verbose.write(text)

def latex_close():
	global latex_terse
	global latex_verbose
	latex_terse.close()
	latex_verbose.close()


def startFTP(log_fname, use_dsu, version):
	"""	Start vsftpd
	
	'version' is an int denoting which version should be run.
	Returns child pid.
	"""
	global vsftpd_pid
	log_f = open(log_fname, "a")
	vsftpd_binary = ""
	if use_dsu:
		vsftpd_binary = modified_root + \
			program_versions[version] + "/vsftpd.so"
	else:
		vsftpd_binary = original_root + \
			program_versions[version] + "/vsftpd"
	log_f.write("Starting vsftpd from:" + vsftpd_binary + "\n\n")	
	log_f.flush()
	ftp_proc = ""
	if use_dsu:  
		ftp_proc = Popen(args=[driver, vsftpd_binary, "../vsftpd.conf"], stdout=log_f)
	else:
		ftp_proc = Popen(args=[vsftpd_binary, "../vsftpd.conf"], stdout=log_f)
	return ftp_proc.pid


def runScript(script, output_fname, time_fname, use_dsu, 
		allow_concurrency = False):
	""" Call a script for benchmarking. 
	
	Saves the data output from the script in output_fname.
	If allow_concurrency == True then we don't wait for the script to finish
	running.
	"""
	output_f = open(output_fname, "a")
	script = bench_directory + script
	output_f.write("Starting script " + script + " :\n")
	output_f.flush()
	script_proc = ""	
	args = [script, str(use_dsu), time_fname, str(number_of_runs)]
	if allow_concurrency == False:
		script_proc = Popen(args, stdout=PIPE)
		stdout_value = script_proc.communicate()[0]
	else:
		script_proc = Popen(args, stdout=PIPE)

	# if measuring time, don't run tests concurrently	
	if allow_concurrency == False:
		script_proc.wait()
		output_f.write(stdout_value)
		output_f.write("\n")
		output_f.close()
		return -1
	else:
		output_f.close()
		return script_proc.pid


def recordMemory(log_fname, pid):
	""" Get the memory usage of a process from /proc/[pid]/status """
	log_f = open(log_fname, "a")
	status_f = open("/proc/" + str(pid) + "/status")
	data = status_f.read()
	log_f.write(data + "\n\n")
	status_f.close()
	log_f.close()
	return data.split("\n")


def doUpdate(log_fname, use_dsu, next_version):
	""" Update to the next version of the program if it exists.
	
	Arguments:
	log_fname:string -- file to store output
	use_dsu:boolean -- call the update_vsftpd.sh in the next vsftpd version
		directory if true or kill the current server and boot the next version
		if False
	next_version -- a number indicating which version to update to from 
		program_versions
	"""
	global program_versions
	global vsftpd_pid
	if next_version > len(program_versions) - 1:
		print "Error: No more versions to upgrade to!"
		exit()

	log_f = open(log_fname, "a"
)
	cwd = os.getcwd()
	upd_script = ""
	if use_dsu:
		upd_script = modified_root + program_versions[next_version] +\
			"/update_vsftpd.sh"
		# change dir to directory with update script
		os.chdir(modified_root + program_versions[next_version])
	else:
		os.kill(vsftpd_pid, 9)
		upd_script = original_root + \
			program_versions[next_version] + "/vsftpd"
	proc = Popen(args=upd_script, stdout=log_f)
	os.chdir(cwd)
	f.close()
	time.sleep(0.1)

	print "Updated to " + program_versions[next_version]
	if use_dsu == False:
		return proc.pid	


def runOneVersionTestsSerial(version_num, use_dsu, time_fname, \
		output_fname, mem_fname, run_vsftpd = True, vsftpd_pid = -1):	
	"""	Run one vsftpd server doing all tests on it without restarting it.

	All of the tests are run serially (one after the other).
	If run_vsftpd == False, then the function assumes vsftpd has been
	started elsewhere.
	"""
	time_ = -1

	# Run vsftp server if requested (default)
	if run_vsftpd:
		vsftpd_pid = startFTP(output_fname, use_dsu, version_num)
		time.sleep(0.1)

	start_time = time.time()
	runScript(bench_script, output_fname, time_fname, use_dsu)
	time_ = time.time() - start_time

	# record memory
	data = recordMemory(mem_fname, vsftpd_pid)
	if use_dsu:
		print "Kitsune Memory::" + str(data[11].split()) + "  " + \
			str(data[14].split())
	else:
		print "Original Memory::" + str(data[11].split()) + "  " + \
			str(data[14].split())
	
	# kill the server if it started from here
	if run_vsftpd:
		os.kill(vsftpd_pid, 9)
	return time_
	time.sleep(0.1)


def runOneVersionTestsConcurrent(version_num, output_fname, mem_fname, \
		num_conns):
	""" Do concurrent connections benchmarking.

	Run the program and connect num_conns times using concurrent_script. 
	Record memory usage.  Run for both kitsune version and original version.
	"""
	vsftpd_pid = 0
	pids = []

	# Run kitsune vsftp server
	vsftpd_pid = startFTP(output_fname, True, version_num)
	time.sleep(0.1)

	# Run "long" connection script
	for i in xrange(num_conns):
		pid = runScript(concurrent_script, output_fname, "/dev/null", True, True)
		pids.append(pid)
	time.sleep(0.1)
	
	# record memory
	data = recordMemory(mem_fname, vsftpd_pid)
	print "Kitsune Memory::" + str(data[11].split()) + "  " + \
		str(data[14].split())
	
	# kill all the connections
	for pid in pids:
		os.kill(pid, 9)

	# kill the server and run the non kitsune one
	os.kill(vsftpd_pid, 9)
	vsftpd_pid = startFTP(output_fname, False, version_num)
	time.sleep(0.1)

	# Run tests
	for i in xrange(num_conns):
		runScript(concurrent_script, output_fname, "/dev/null/", True, True)
	time.sleep(0.1)			

	# record memory
	data = recordMemory(mem_fname, vsftpd_pid)
	print "Original Memory::" + str(data[11].split()) + "  " + \
		str(data[14].split())
	
	# kill the server
	os.kill(vsftpd_pid, 9)
	time.sleep(0.1)


def runUpdateTests(output_fname, time_fname, mem_fname, number_of_runs):
	""" Benchmark, update, benchmark, update, etc.

	Start with the first version, run the benchmark (serially),
	update, and repeate until last version is reached.
	"""
	next_version = 0
	vsftpd_pid = -1
	times = []

	# Run ftp server
	vsftpd_pid = startFTP(output_fname, True, next_version)
	time.sleep(0.1)
	next_version += 1

	# Run bench_script and then update	
	for i in range(len(program_versions) - 1):			
		runOneVersionTestsSerial(-1, True, time_fname, \
			output_fname, mem_fname, False, vsftpd_pid)
		times.append([])
		times[i] = parseTimelogFile(time_fname, number_of_runs)
		doUpdate(output_fname, True, next_version)
		next_version += 1
		time.sleep(0.1)

	# final run of bench_script
	runOneVersionTestsSerial(-1, True, time_fname, \
		output_fname, mem_fname, False, vsftpd_pid)
	times.append([])
	times[len(program_versions) - 1] = parseTimelogFile(time_fname, number_of_runs)
		  

	time.sleep(0.1)
	# kill the server
	os.kill(vsftpd_pid, 9)
	return times 


def parseTimelogFile(fname, num_runs):
	"""Get an array of times from a file and then clear the file."""
	f = open(fname)
	times = [float(f.readline()) for x in range(num_runs)]
	open(fname, "w").close()
	return times


def parseMemorylogFile(fname, num_runs):
	"""Get VmSize and VmRSS from a memory log file."""
	vmsize = []
	vmrss = []
	f = open(fname)
	for line in f:
		field = line.split()
		if field and field[0] == "VmSize:":
			vmsize.append(field[1])
		if field and field[0] == "VmRSS:":
			vmrss.append(field[1])
	return (vmsize, vmrss)		
	
""" 
Main
"""
if __name__ == '__main__':	
	# check for LaTeX
	livetex_check = Popen(args=["which", "latex"]).wait()
	if livetex_check != 0:
		print "You don't have livetex installed.\nExiting."
		exit()

	conf = loadConf()

	OUTPUT_LOG = results_directory + conf["BenchSettings"]["OutputLog"]
	TIME_LOG_KITSUNE = results_directory + conf["BenchSettings"]["TimeLogKitsune"]
	TIME_LOG_ORIGINAL = results_directory + \
		conf["BenchSettings"]["TimeLogOriginal"]
	MEMORY_LOG_KITSUNE = results_directory + conf["BenchSettings"]["MemLogKitsune"]
	MEMORY_LOG_ORIGINAL = results_directory + \
		conf["BenchSettings"]["MemLogOriginal"]
		
	original_times = [[] for i in xrange(len(program_versions))]
	kitsune_times = [[] for i in xrange(len(program_versions))]

	# clear files
	open(OUTPUT_LOG, "w").close()
	open(TIME_LOG_KITSUNE, "w").close()
	open(TIME_LOG_ORIGINAL, "w").close()
	open(MEMORY_LOG_KITSUNE, "w").close()
	open(MEMORY_LOG_ORIGINAL, "w").close()

	time_diff = 0
	
	print "\n"
	print \
""" __                           ___                
(_ _|_  _. ._ _|_ o ._   _     |  _   _ _|_  _ o 
__) |_ (_| |   |_ | | | (_|    | (/_ _>  |_ _> o 
                         _|                      
"""
	latex_init()
	latex_both(
"""
\\documentclass[12pt,letterpaper]{article}
\\usepackage{graphicx}
\\title{VSFTPD Benchmark Results}
\\author{Michail Denchev $<$mdenchev@gmail.com$>$}
\\date{\\today}

\\addtolength{\\oddsidemargin}{-.800in}
\\addtolength{\\evensidemargin}{-.800in}
\\addtolength{\\textwidth}{1.60in}

\\addtolength{\\topmargin}{-.800in}
\\addtolength{\\textheight}{1.60in}

\\begin{document}
\\maketitle

These results are generated from kitsune/examples/vsftpd/ftp-tests/run\_kitsune\_benchmark.py. \\\\

The versions being run are: %(versions)s \\\\

All executions of vsFTPd use the two process model. \\\\

\\section{Single Version Serial Tests $($%(runs)i Runs$)$}
Command list used is %(bench_script)s. \\\\

This benchmark measures how long a set of tests takes to run N times on a freshly started version. The idea behind it is to see if regular operations take longer on the Kitsune version than on the original. The time it takes to start vsftpd is not included. \\

\\begin{center}
\\begin{tabular}{ | c | c | c | c | c | }
\\hline
Version & Kitsune Median (s) & Original Median (s) & Diff (Ek - Orig) (s) & Change (\\%%) \\\\
\\hline
	
""" %{
	'versions' : str(program_versions).replace("_","\_"),
	'runs' : number_of_runs,
	'bench_script' : str(bench_script).replace("_","\_")
	})

	print "Running Single Version Serial Tests (%i Runs)" %number_of_runs
	figure(figsize=(10, 30))
	for i in xrange(len(program_versions)):
		print "Version " + program_versions[i]
		
		runOneVersionTestsSerial(i, True, TIME_LOG_KITSUNE, OUTPUT_LOG, \
			MEMORY_LOG_KITSUNE, number_of_runs)
		kitsune_times[i] = parseTimelogFile(TIME_LOG_KITSUNE, number_of_runs)
		runOneVersionTestsSerial(i, False, TIME_LOG_ORIGINAL, OUTPUT_LOG, \
			MEMORY_LOG_ORIGINAL, number_of_runs)  	
		original_times[i] = parseTimelogFile(TIME_LOG_ORIGINAL, number_of_runs)
		
		# draw a boxplot of the data
		subplot(8,2,i+1)
		hist(kitsune_times[i])

		kitsune_cumulative_time = numpy.median(kitsune_times[i])
		original_cumulative_time = numpy.median(original_times[i])
		time_diff = kitsune_cumulative_time - original_cumulative_time	
		percent_diff = ((kitsune_cumulative_time/original_cumulative_time - 1)*100)
		latex_both(
"""\
%s & %f & %f & %f & %.2f \\%%\\\\
\\hline
	
""" %(program_versions[i], kitsune_cumulative_time, original_cumulative_time,\
			time_diff, percent_diff)
		)
		print "Test list was: " + str(bench_script)
		print "Time difference in terms of original vsftpd (mean): %.2f" % \
			percent_diff + "%"
	
	savefig("SerialBench")
	print "\n",

	### End time table, start vmsize memory table
	latex_both(
"""
\\end{tabular}
\\end{center}

This test also records memory usage of the server taken from /proc/[pid]/status. This is done at the end of execution of the tests.
\\begin{center}
\\begin{tabular}{ | c | c | c | c | c |}
\\hline
Version & Ek. VmSize (kB) & Orig. VmSize (kB) & Diff (Ek - Orig)  (kB) & Diff ($\\Delta$/Orig) (\\%)\\\\
\\hline
""")
	
	#parseMemorylogFile returns a tuple (vmsize, vmrss)
	kitsune_mem = parseMemorylogFile(MEMORY_LOG_KITSUNE, number_of_runs)
	original_mem = parseMemorylogFile(MEMORY_LOG_ORIGINAL, number_of_runs)
	
	for i, version in enumerate(program_versions):
		diff = int(kitsune_mem[0][i]) - int(original_mem[0][i])
		latex_both(
"""	
%s & %s & %s & %d & %.2f \\%%\\\\
\\hline
""" %(version, kitsune_mem[0][i], original_mem[0][i], \
			diff, diff/float(original_mem[0][i]) * 100)
	)
	## start RSS memory table
	latex_both(
"""
\\end{tabular}
\\end{center}

\\begin{center}
\\begin{tabular}{ | c | c | c | c | c | }
\\hline
Version & Ek. VmRSS (kB) & Orig. VmRSS (kB) & Diff (Ek - Orig) (kB) & Diff ($\\Delta$/Orig) (\\%)\\\\
\\hline
""")

	for i, version in enumerate(program_versions):
		diff = int(kitsune_mem[1][i]) - int(original_mem[1][i])	
		latex_both(
"""	
%s & %s & %s & %d & %.2f \\%%\\\\
\\hline
""" %(version, kitsune_mem[1][i], original_mem[1][i], \
			diff, diff/float(original_mem[1][i]) * 100)
		)

	latex_both(
"""
\\end{tabular}
\\end{center}
""")

	### Heading and table start for next sections
	latex_both(
"""
\\section{Update Tests $($%(runs)i Runs$)$}
Command list used is %(bench_script)s. \\\\

This benchmark does the same thing as the above "Single Version" benchmark with the only difference being that instead of starting each version separately, it updates from the previous version (original-vsftpd comparison data is taken from last benchmark). \\\\

\\begin{center}
\\begin{tabular}{ | c | c | c | c | c | }
\\hline
Upd. To Version & Kitsune Median (s) & Original Median (s) & Diff (Ek - Orig) (s) & Change (\\%%) \\\\
\\hline

""" %{
	'runs' : number_of_runs,
	'bench_script' : str(bench_script).replace("_","\_")
	})

	f = open("results/UPDATE_DATA", "w")
	f.write("--\n")
	f.close()
	open("results/UPDATE_STATUS_DATA", "w").close()
	
	print "Running Update Serial Tests: "
	print "Starting with " + program_versions[0]
	kitsune_times = runUpdateTests(OUTPUT_LOG, TIME_LOG_KITSUNE, \
		MEMORY_LOG_KITSUNE, number_of_runs)
	print "Time differences between kitsune updated up to X and original:"
	for i, version in enumerate(program_versions):
		diff = median(kitsune_times[i]) - median(original_times[i])
		print "%i: %.2f" %(i, (median(kitsune_times[i])/median(original_times[i]) - 1)*100) \
			+ "%"
		latex_both(
"""	
%s & %s & %s & %f & %.2f \\%%\\\\
\\hline
""" %(version, median(kitsune_times[i]), median(original_times[i]), \
			diff, diff/median(original_times[i]) * 100)
		)

	### End time table, start vmsize memory table
	latex_both(
"""
\\end{tabular}
\\end{center}

This test also records memory usage of the server taken from /proc/[pid]/status. This is done at the end of execution of the tests.
\\begin{center}
\\begin{tabular}{ | c | c | c | c | c | }
\\hline
Upd. To Version & Ek. VmSize (kB) & Orig. VmSize (kB) & Diff (Ek - Orig)  (kB) & Diff ($\\Delta$/Orig) (\\%)\\\\
\\hline
""")
	
	# parseMemorylogFile returns a tuple (vmsize, vmrss)
	kitsune_mem = parseMemorylogFile(MEMORY_LOG_KITSUNE, number_of_runs)
	
	for i, version in enumerate(program_versions):
		diff = int(kitsune_mem[0][i]) - int(original_mem[0][i])
		latex_both(
"""	
%s & %s & %s & %d & %.2f \\%%\\\\
\\hline
""" %(version, kitsune_mem[0][i], original_mem[0][i], \
			diff, diff/float(original_mem[0][i]) * 100)
		)
	### start RSS memory table
	latex_both(
"""
\\end{tabular}
\\end{center}

\\begin{center}
\\begin{tabular}{ | c | c | c | c | c | }
\\hline
Upd. To Version & Ek. VmRSS (kB) & Orig. VmRSS (kB) & Diff (Ek - Orig) (kB) & Diff ($\\Delta$/Orig) (\\%)\\\\
\\hline
""")

	for i, version in enumerate(program_versions):
		diff = int(kitsune_mem[1][i]) - int(original_mem[1][i])	
		latex_both(
"""	
%s & %s & %s & %d & %.2f \\%%\\\\
\\hline
""" %(version, kitsune_mem[1][i], original_mem[1][i], \
			diff, diff/float(original_mem[1][i]) * 100)
		)

	print "\n",

	latex_both(
"""
\\end{tabular}
\\end{center}

\\section{Update Time and Memory Benchmark }
No commands issued. \\\\

This benchmark times how long it takes for the standalone process to update from start to finish recording memory just prior to releasing the old .so file. This is done by modifying kitsune.c with the code found in ftp-tests/TEST. No connections are made during this test.

\\begin{center}
\\begin{tabular}{ | c | c | c | c | }
\\hline
Upd. To Version & VmSize (kB) & VmRSS (kB) & Upd. Time (ms) \\\\
\\hline
""")		

	f = open("results/UPDATE_DATA", "r")
	times = f.read().split()
	f.close()
	
	# this is a bit redundant since this test is already run
	kitsune_times = runUpdateTests(OUTPUT_LOG, TIME_LOG_KITSUNE, \
		MEMORY_LOG_KITSUNE, number_of_runs)

	kitsune_mem = parseMemorylogFile("results/UPDATE_STATUS_DATA", number_of_runs)

	for i, version in enumerate(program_versions):	
		latex_both(
"""	
%s & %s & %s & %s \\\\
\\hline
""" %(version, kitsune_mem[0][i], original_mem[1][i], \
			times[i])
		)

	latex_both(
"""
\\end{tabular}
\\end{center}

\\section{Concurrent Connections Benchmark $($%(conns)i Connections$)$}
Test used is %(test)s. \\\\

This benchmark records the memory used when running N connections simultaneously. The connection either idle for a few seconds (connect) or alternatively download a file for however long it takes to download (download\\_binary). This measures if having a large amount of connections causes a significant difference in memory between modified and non-modified vsftpd. Each version is started separately.

\\begin{center}
\\begin{tabular}{ | c | c | c | c | c | }
\\hline
Version & Ek. VmSize (kB) & Orig. VmSize (kB) & Diff (Ek - Orig) (kB) & Diff ($\\Delta$/Orig) (\\%%)\\\\
\\hline
""" %{
	'conns' : number_of_connections,
	'test' : str(concurrent_script).replace("_","\_")
	})
	print "Running Single Version Concurrency Test (%i CONNECTIONS)" \
		%number_of_connections
	print "Test used: " + concurrent_script
	for i in range(len(program_versions)):
		runOneVersionTestsConcurrent(i, OUTPUT_LOG, MEMORY_LOG_KITSUNE, \
			number_of_connections)

	# parseMemorylogFile returns a tuple (vmsize, vmrss)
	kitsune_mem = parseMemorylogFile(MEMORY_LOG_KITSUNE, number_of_runs)
	original_mem = parseMemorylogFile(MEMORY_LOG_ORIGINAL, number_of_runs)

	for i, version in enumerate(program_versions):
		diff = int(kitsune_mem[0][i]) - int(original_mem[0][i])
		latex_both(
"""	
%s & %s & %s & %d & %.2f \\%%\\\\
\\hline
""" %(version, kitsune_mem[0][i], original_mem[0][i], \
			diff, diff/float(original_mem[0][i]) * 100)
		)
	### start RSS memory table
	latex_both(
"""
\\end{tabular}
\\end{center}

\\begin{center}
\\begin{tabular}{ | c | c | c | c | c | }
\\hline
Version & Ek. VmRSS (kB) & Orig. VmRSS (kB) & Diff (Ek - Orig) (kB) & Diff ($\\Delta$/Orig) (\\%)\\\\
\\hline
""")

	for i, version in enumerate(program_versions):
		diff = int(kitsune_mem[1][i]) - int(original_mem[1][i])	
		latex_both(
"""	
%s & %s & %s & %d & %.2f \\%%\\\\
\\hline
""" %(version, kitsune_mem[1][i], original_mem[1][i], \
			diff, diff/float(original_mem[1][i]) * 100)
		)
	

	latex_both(
"""
\\end{tabular}
\\end{center}
\\end{document}
""")

  # Convert tex files to pdf
	os.chdir("results")
	p1 = Popen(args=["pdflatex", "latex_terse.tex"])
	p2 = Popen(args=["pdflatex", "latex_verbose.tex"])
	
	latex_close()

