#!/usr/bin/env ruby 

require 'net/ftp'
require 'pp'
require 'benchmark'

$connect_args = {
  :host => "localhost",
  :port => 2021,
  :user => "anonymous",
#  :password => "ftp",
  :debug => true,
}

$number_of_runs = ARGV[0].to_i
$time_f = File.open(ARGV[1], "a")
$ftp_work_folder = ARGV[2]
$test_to_run = ARGV[3]
$use_alternate_binfile = ARGV[3] == "dl" && ARGV[4] == "yes"

$ascii_copy_file = $ftp_work_folder + "/32_byte_file"
$bin_copy_file = $ftp_work_folder + "/32_byte_file"
$alternate_bin_copy_file = $ftp_work_folder + "/empty_file"
$bin_dest_file = "bin.dat"
$ascii_dest_file = "asc.dat"
$create_file = "empty.dat"
$ftp_start_folder = "/"
$junk_file = $ftp_work_folder + "/junk.dat"
$rename_file = "rename.dat"
$mkdir_folder = "test_mkdir"

class Object 
  def current_method 
    caller[0] =~ /\d:in `([^']+)'/ 
    $1 
  end
end

def record_memory(fname)
	status = File.open("/proc/" + $$.to_s + "/status").read()
	mem_f.write(status + "\n\n")
end

def do_in_connection(args, name)
  begin
    ftp = Net::FTP.new()
    # ftp.debug_mode = args[:debug] == true
    ftp.connect(args[:host], args[:port])
    ftp.login(args[:user], args[:password])
    yield ftp
	ftp.quit()
  rescue 
    puts "Test (#{name}) Failed: #{$!}"
    exit 1
  end
end

# benchmark script that combines multiple actions
def bench_cmds(args)
  do_in_connection(args, current_method) { |ftp|
    if $test_to_run == "ls" 
      test_cmds_ls(ftp)
    elsif $test_to_run == "dl" 
      test_download_binary(ftp)
    end 
  }
end

def test_connect(ftp)
  do_in_connection(args, current_method) { |ftp| }
end

def test_noop(ftp)
 	ftp.noop()
end

def test_pwd(ftp)
    result = ftp.pwd()
    raise "Unexpected result from PWD" if result != $ftp_start_folder
end

def test_chdir(ftp)
    ftp.chdir($ftp_work_folder)
    result = ftp.pwd()
    puts(result)
    raise "Unexpected result from PWD" if result != $ftp_work_folder
end

def setup_files
  system("touch #{$ftp_work_folder}/#{$create_file}")
  $bincpyf = $bin_copy_file
  if $test_to_run == "dl" && $use_alternate_binfile
    $bincpyf = $alternate_bin_copy_file
  end 
  system("cp #{$bincpyf} #{$ftp_work_folder}/#{$bin_dest_file}")
  system("cp #{$ascii_copy_file} #{$ftp_work_folder}/#{$ascii_dest_file}")
  # system("dd if=/dev/zero of=#{$ascii_dest_file} count=32 bs=1")
end

def clear_files
  file1 = $ftp_work_folder + "/" + $bin_dest_file
  file2 = $ftp_work_folder + "/" + $ascii_dest_file
  file3 = $ftp_work_folder + "/" + $create_file
  system("rm -f #{file1}")
  system("rm -f #{file2}")
  system("rm -f #{file3}")
  system("rm -f #{file1}-ren")
  system("rm -f #{file2}-ren")
  system("rm -f #{file3}-ren")
end

def compare_files(path1, path2)
  0 == system("diff -q #{path1} #{path2}")
end

def check_transfer(file1, file2)
  raise "Binary File does not match." if compare_files(file1, file2)
end

def test_cmds_ls(ftp)
  ftp.chdir($ftp_work_folder)
  result = ftp.ls()
end

def test_upload_ascii(ftp)
  	clear_files
    ftp.puttextfile($ascii_copy_file, $ftp_work_folder + "/" + $ascii_dest_file)
    check_transfer($ascii_copy_file, $ftp_work_folder + "/" + $ascii_dest_file)
end

def test_upload_binary(ftp)
	clear_files
    ftp.putbinaryfile($bin_copy_file, $ftp_work_folder + "/" + $bin_dest_file)
    check_transfer($bin_copy_file, $ftp_work_folder + "/" + $bin_dest_file)
end

def test_download_ascii(ftp)
  	clear_files
  	setup_files
    ftp.chdir($ftp_work_folder)
    ftp.gettextfile($ftp_work_folder + "/" + $ascii_dest_file, $junk_file)
    check_transfer($ftp_work_folder + "/" + $ascii_dest_file, $junk_file)
end

def test_download_binary(ftp)
  	clear_files
  	setup_files
    work_folder = $ftp_work_folder
    ftp.chdir($ftp_work_folder)
    ftp.getbinaryfile($ftp_work_folder + "/" + $bin_dest_file, $junk_file)
    check_transfer($ftp_work_folder + "/" + $bin_dest_file, $junk_file)
end

def check_size(ftp, full_name)
  raise "File size mismatch #{full_name}" if ftp.size(full_name) != File.size(full_name)
end

def test_file_size(ftp)
  	clear_files
  	setup_files
    check_size(ftp, $ftp_work_folder + "/" + $create_file)
    check_size(ftp, $ftp_work_folder + "/" + $bin_dest_file)
    check_size(ftp, $ftp_work_folder + "/" + $ascii_dest_file)
end

def test_rename(ftp)
  	setup_files
    file1 = $ftp_work_folder + "/" + $bin_dest_file
    file2 = $ftp_work_folder + "/" + $ascii_dest_file
    file3 = $ftp_work_folder + "/" + $create_file
    ftp.rename(file1, file1 + "-ren")
    check_exists(file1 + "-ren", true)
    check_exists(file1, false)
    ftp.rename(file2, file2 + "-ren")
    check_exists(file2 + "-ren", true)
    check_exists(file2, false)
    ftp.rename(file3, file3 + "-ren")
    check_exists(file3 + "-ren", true)
    check_exists(file3, false)
end

def test_delete(ftp)
	setup_files
	ftp.delete($ftp_work_folder + "/" + $bin_dest_file)
	check_exists($ftp_work_folder + "/" + $bin_dest_file, false)
	ftp.delete($ftp_work_folder + "/" + $ascii_dest_file)
	check_exists($ftp_work_folder + "/" + $ascii_dest_file, false)
	ftp.delete($ftp_work_folder + "/" + $create_file)
	check_exists($ftp_work_folder + "/" + $create_file, false)
end

def check_exists(path, exists)
  raise "File/Path exists-#{exists} failure: #{path}" if File.exist?(path) != exists
end

def test_mkdir_rmdir(ftp)
    full_mkdir_folder = $ftp_work_folder + "/" + $mkdir_folder
    ftp.mkdir(full_mkdir_folder)
    check_exists(full_mkdir_folder, true)
    ftp.rmdir(full_mkdir_folder)  
    check_exists(full_mkdir_folder, false)
end

time = Benchmark.realtime do
  for i in (1..$number_of_runs)
    bench_cmds($connect_args)
  end
end
$time_f.write(time.to_s + "\n")
$stderr.print(time.to_s + "\n")
