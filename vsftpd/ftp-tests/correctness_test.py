# code by michail denchev <mdenchev@gmail.com>

import time, os, signal, sys, Queue, thread
from subprocess import *
program_start = time.time()

# Some useful paths and lists
kitsune_driver = "/home/nqe/kitsune/bin/driver"
kitsune_vsftpd_root = "/home/nqe/kitsune/examples/vsftpd/"
original_vsftpd_root = "/home/nqe/kitsune/examples/vsftpd/orig_vsftpd/"
vsftpd_versions = ["vsftpd-1.1.0", "vsftpd-1.1.1", "vsftpd-1.1.2",
"vsftpd-1.1.3", "vsftpd-1.2.0", "vsftpd-1.2.1", 	
"vsftpd-1.2.2", "vsftpd-2.0.0", "vsftpd-2.0.1", "vsftpd-2.0.2",
"vsftpd-2.0.3", "vsftpd-2.0.4", "vsftpd-2.0.5", "vsftpd-2.0.6"]

test_directory = "/home/nqe/kitsune/trunk/examples/vsftpd/ftp-tests/"
test_list = ["./ftp-test.rb"] # this is the list of tests that will be run for timing
concurrent_test = "connect" # test used in concurrency testing

"""
Useful code for running concurrent tests. It supports actively waiting on
multiple processes to finish. Uses threads. Stolen from:
http://stackoverflow.com/questions/100624/python-on-windows-how-to-wait-for-multiple-child-processes
"""
process_count = 0 # number of processes
results = Queue.Queue()
def process_waiter(poepn, description, que):
	try: popen.wait()
	finally: que.put( (description, popen.returncode))

def waitForTests():
	global process_count
	while process_count > 0:
		description, rc = results.get()
		print "job", description, "ended with rc =", rc
    	process_count -= 1

"""
Starts original or modified vsftpd depending on the value of use_kitsune.
'version' is an int denoting which version should be run.
Returns child pid.
"""
def startFTP(log_filename, use_kitsune, version):
	global vsftpd_pid
	log_file = open(log_filename, "a")
	vsftpd_bin = ""
	if use_kitsune:
		vsftpd_bin = kitsune_vsftpd_root + \
			vsftpd_versions[version] + "/vsftpd.so"
	else:
		vsftpd_bin = original_vsftpd_root + \
			vsftpd_versions[version] + "/vsftpd"
	log_file.write("Starting vsftpd from:" + vsftpd_bin + "\n\n")	
	log_file.flush()
	ftp_proc = ""
	if use_kitsune:
		ftp_proc = Popen(args=[kitsune_driver, vsftpd_bin], stdout=log_file)
	else:
		ftp_proc = Popen(args=[vsftpd_bin], stdout=log_file)
	return ftp_proc.pid


""" 
Calls an already written test using the linux time command. Saves the
data in two separate files, one for the output of time and one for the 
output of the test.
"""
def runTest(test, testlog_filename, timelog_filename, allow_concurrency = False):
	#testlog_file = open(testlog_filename, "a")
	#test = test_directory + test
	#testlog_file.write("Starting test " + test + ":\n")
	#testlog_file.flush()
	#test_p = ""
	#if timelog_filename:
	#	test_p = Popen(args=["time", "-ap", "-o", test_directory + \
	#		timelog_filename, test], stdout=testlog_file)
	#else:
	test_p = Popen(args=[test])
		
	# if measuring time lapses don't run tests concurrently	
	#if allow_concurrency == False: 
	test_p.wait()
	if test_p.returncode == 1: exit(1)
	#testlog_file.write("\n")
	#testlog_file.close()

"""
Gets the memory usage of a process from /proc/[pid]/status
"""
def recordMemory(output_file, pid):
	output_f = open(output_file, "a")
	status_f = open("/proc/" + str(pid) + "/status")
	data = status_f.read()	
	output_f.write(data + "\n\n")
	status_f.close()
	output_f.close()
	return data.split("\n")

""" 
Updates to the next version of vsftpd (if it exists).
If use_kitsune == True: call the update_vsftpd.sh in the next vsftpd version directory. 
Else: kill the current server and boot the next version.
"""
def doUpdate(log_file, use_kitsune, next_version):
	global vsftpd_versions
	global vsftpd_pid
	if next_version > len(vsftpd_versions) - 1:
		print "No more version to upgrade to!"
		return

	f = open(log_file, "a")
	bin = ""
	cwd = os.getcwd()
	if use_kitsune:
		bin = kitsune_vsftpd_root + vsftpd_versions[next_version] +\
			"/update_vsftpd.sh"
		# change dir to directory with update script
		os.chdir(kitsune_vsftpd_root + vsftpd_versions[next_version])
	else:
		os.kill(vsftpd_pid, 9)
		bin = original_vsftpd_root + \
			vsftpd_versions[next_version] + "/vsftpd"
	proc = Popen(args=bin, stdout=f)
	os.chdir(cwd)
	f.close()
	
	print "Updated to " + vsftpd_versions[next_version]
	if use_kitsune == False:
		return proc.pid	

"""
Runs one vsftpd server doing all tests on it without restarting it.
All of the tests are run serially (one after the other).
@ if start_vsftpd == False, then the function assumes vsftpd has been
started elsewhere.
"""
def runOneVersionTestsSerial(version_num, use_kitsune, timelog_fn, \
		testlog_fn, memory_fn, number_of_runs, run_vsftpd = True, \
		vsftpd_pid = -1):	
	cumulative_time = 0

	# Run vsftp server if requested (default)
	if run_vsftpd:
		vsftpd_pid = startFTP(testlog_fn, use_kitsune, version_num)
		time.sleep(0.05)

	# Run tests
	for i in xrange(number_of_runs):
		for test in test_list:
			start_time = time.time()
			runTest(test, testlog_fn, timelog_fn)
			end_time = time.time() - start_time
			cumulative_time += end_time

	# record memory
	data = recordMemory(memory_fn, vsftpd_pid)
	if use_kitsune:
		print "Kitsune Memory::" + str(data[11].split()) + "  " + \
			str(data[14].split())
	else:
		print "Original Memory::" + str(data[11].split()) + "  " + \
			str(data[14].split())
	
	# kill the server if it started from here
	if run_vsftpd:
		os.kill(vsftpd_pid, 9)
	return cumulative_time

"""
Runs one vsftpd server doing all tests on it without restarting it.
All of the tests are run concurrently (at the same time).
"""
def runOneVersionTestsConcurrent(version_num, testlog, memorylog, num_conns):
	vsftpd_pid = 0

	# Run kitsune vsftp server
	vsftpd_pid = startFTP(testlog, True, version_num)
	time.sleep(0.05)

	# Run tests (these tests should kill themselves after a while)
	for i in xrange(num_conns):
		runTest(concurrent_test, testlog, None, True)
	time.sleep(1.0)
	
	# record memory
	data = recordMemory("results/memory_e", vsftpd_pid)
	print "Kitsune Memory::" + str(data[11].split()) + "  " + \
		str(data[14].split())
	
	# kill the server and run the non kitsune one
	os.kill(vsftpd_pid, 9)
	vsftpd_pid = startFTP(testlog, False, version_num)
	time.sleep(0.05)

	# Run tests
	for i in xrange(num_conns):
		runTest(concurrent_test, testlog, None, True)
	time.sleep(1.0)			

	# record memory
	data = recordMemory("results/memory_r", vsftpd_pid)
	print "Original Memory::" + str(data[11].split()) + "  " + \
		str(data[14].split())
	
	# kill the server
	os.kill(vsftpd_pid, 9)


""" 
Benchmarks updated versions.
Starts with the first version, runs the tests (serially),
updates, and repeates until last version is reached.
Returns list of cumulative times from the tests.
Note, the version number gets appended to the provided memory 
filename.
"""
def runUpdateTests(testlog, timelog, memorylog, number_of_runs):
	vsftpd_pid = -1
	vsftpd_next_version = 0 # start version
	cumulative_times = [None for i in range(len(vsftpd_versions))]

	# Run ftp server
	vsftpd_pid = startFTP(testlog, True, vsftpd_next_version)
	time.sleep(0.05)
	vsftpd_next_version += 1

	# Run tests and then update	
	for i in range(len(vsftpd_versions) - vsftpd_next_version):			
		result = runOneVersionTestsSerial(-1, True, timelog, \
			testlog, memorylog, number_of_runs, False, vsftpd_pid)
		cumulative_times[vsftpd_next_version - 1] = result
			
		# run an update and wait a bit to make sure it's complete
		doUpdate(testlog, True, vsftpd_next_version)
		vsftpd_next_version += 1
		time.sleep(0.05)

	# final run through the tests
	result = runOneVersionTestsSerial(-1, True, timelog, \
		testlog, memorylog, number_of_runs, False, vsftpd_pid)
	cumulative_times[vsftpd_next_version - 1] = result

	# kill the server
	os.kill(vsftpd_pid, 9)
	return cumulative_times

"""
Sums up real, user, and sys time and returns them.
"""
def parseTimelogFile(file_name):
	real = 0.0
	user = 0.0
	sys = 0.0
	f = open(file_name)
	for line in f:
		line = line.split()
		if line[0] == "real": real += float(line[1])
		elif line[0] == "user": user += float(line[1])
		elif line[0] == "sys": sys += float(line[1])
		else: print "parseTimelogiFile error"
	return (real, user, sys)

""" 
Main---
Note: There are (will be) two additional tests inside the kitsune code to record memory in the middle of an update, and the time it takes to finish an update. 
"""
if __name__ == '__main__':	
	NUMBER_OF_RUNS = 1
	NUMBER_OF_CONNECTIONS = 30
	TIME_LOG_KITSUNE = "results/time_output_kitsune"
	TIME_LOG_ORIGINAL = "results/time_output_original"
	TEST_LOG = "results/tests_output"
	MEMORY_LOG_KITSUNE = "results/kitsune_memory"
	MEMORY_LOG_ORIGINAL = "results/original_memory"
	
	original_times = [None for i in xrange(len(vsftpd_versions))]
	# clear files
	open(TIME_LOG_KITSUNE, "w").close()
	open(TIME_LOG_ORIGINAL, "w").close()
	open(TEST_LOG, "w").close()

	time_diff = 0
	kitsune_cumulative_time = 0.0
	original_cumulative_time = 0.0
	
	print "\n"
	print \
""" __                           ___                
(_ _|_  _. ._ _|_ o ._   _     |  _   _ _|_  _ o 
__) |_ (_| |   |_ | | | (_|    | (/_ _>  |_ _> o 
                         _|                      
"""
	print "Running Single Version Serial Tests (%i RUNS)" %NUMBER_OF_RUNS
	#for i in xrange(len(vsftpd_versions)):
	#	print "Version " + vsftpd_versions[i]
		#kitsune_cumulative_time = runOneVersionTestsSerial(i, True, \
		#	TIME_LOG_KITSUNE, TEST_LOG, "/dev/nul", NUMBER_OF_RUNS)
		#original_cumulative_time = runOneVersionTestsSerial(i, False, \
		#	TIME_LOG_ORIGINAL, TEST_LOG, "/dev/nul", NUMBER_OF_RUNS)
		#original_times[i] = original_cumulative_time
		#time_diff = kitsune_cumulative_time - original_cumulative_time	
		#print "Test list was: " + str(test_list)
		#print "Time difference in terms of original vsftpd (mean): %.2f" % \
		#	((kitsune_cumulative_time/original_cumulative_time - 1)*100) + "%"
		# The below prints data from the linux time command, but it doesn't
		# seem very useful
		#kitsune_time_data = parseTimelogFile(TIME_LOG_KITSUNE)
		#original_time_data = parseTimelogFile(TIME_LOG_ORIGINAL)
		#print "Results from Linux time program::"
		#print "Kitsune time data (real, user, sys):"
		#for i in kitsune_time_data: print "%.4f  "%i,
		#print "\nOriginal time data (real, user, sys):"
		#for i in original_time_data: print "%.4f  "%i,
		#print "\nDelta (real, user, sys):"
		#for i in range(3): print "%.4f  "%(kitsune_time_data[i] - \
		#	original_time_data[i]),		
	#print "\n",

	print "Running Update Serial Tests: "
	print "Starting with " + vsftpd_versions[0]
	kitsune_times = runUpdateTests(TEST_LOG+"_u", TIME_LOG_KITSUNE+"_u", \
		MEMORY_LOG_KITSUNE+"_u", NUMBER_OF_RUNS)
	#print "Time differences between kitsune updated up to X and original:"
	#for i in xrange(len(vsftpd_versions)):
	#	print "%i: %.2f" %(i, (kitsune_times[i]/original_times[i] - 1)*100) \
	#		+ "%"
	#print "\n",

	#print "Running Single Version Concurrency Test (%i CONNECTIONS)" \
	#	%NUMBER_OF_CONNECTIONS
	#print "Test used: " + concurrent_test
	#runOneVersionTestsConcurrent(0, TEST_LOG, "implementthis", \
	#	NUMBER_OF_CONNECTIONS)
