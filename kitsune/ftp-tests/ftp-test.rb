#!/usr/bin/env ruby 

require 'net/ftp'

$connect_args = {
  :host => "localhost",
  :port => 2021,
  :user => "anonymous",
#  :password => "ftp",
  :debug => true,
}

$ftp_work_folder = "/tmp/ftptest"
$bin_copy_file = "/bin/ls"
$ascii_copy_file = "/usr/share/dict/words"
$bin_dest_file = "bin.dat"
$ascii_dest_file = "asc.dat"
$create_file = "empty.dat"
$ftp_start_folder = "/"
$junk_file = "/tmp/junk.dat"
$rename_file = "rename.dat"
$mkdir_folder = "test_mkdir"


class Object 
  def current_method 
    caller[0] =~ /\d:in `([^']+)'/ 
    $1 
  end 
end

def do_in_connection(args, name)
  begin
    ftp = Net::FTP.new()
    ftp.debug_mode = args[:debug] == true
    ftp.connect(args[:host], args[:port])
    ftp.login(args[:user], args[:password])
    yield ftp
    ftp.quit()
  rescue 
    puts "Test (#{name}) Failed: #{$!}"
    exit 1
  end
end

# This test simply connects and disconnects from the server.
def test_connect(args)
  do_in_connection(args, current_method) { |ftp| true }
end

def test_noop(args)
  do_in_connection(args, current_method) { |ftp| ftp.noop() }
end

def test_pwd(args)
  do_in_connection(args, current_method) { |ftp|
    result = ftp.pwd()
    raise "Unexpected result from PWD" if result != $ftp_start_folder
  }
end

def test_chdir(args)
  do_in_connection(args, current_method) { |ftp|
    ftp.chdir($ftp_work_folder)
    result = ftp.pwd()
    puts(result)
    raise "Unexpected result from PWD" if result != $ftp_work_folder
  }
end

def setup_files
  system("touch #{$ftp_work_folder}/#{$create_file}")
  system("cp #{$bin_copy_file} #{$ftp_work_folder}/#{$bin_dest_file}")
  system("cp #{$ascii_copy_file} #{$ftp_work_folder}/#{$ascii_dest_file}")
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

def test_cmds_ls(args)
  do_in_connection(args, current_method) { |ftp|
    work_folder = $ftp_work_folder
    ftp.chdir($ftp_work_folder)
    result = ftp.ls()
  }
end

def test_upload_ascii(args)
  clear_files
  do_in_connection(args, current_method) { |ftp|
    ftp.puttextfile($ascii_copy_file, $ftp_work_folder + "/" + $ascii_dest_file)
    check_transfer($ascii_copy_file, $ftp_work_folder + "/" + $ascii_dest_file)
  }
end

def test_upload_binary(args)
  clear_files
  do_in_connection(args, current_method) { |ftp|
    ftp.putbinaryfile($bin_copy_file, $ftp_work_folder + "/" + $bin_dest_file)
    check_transfer($bin_copy_file, $ftp_work_folder + "/" + $bin_dest_file)
  }
end

def test_download_ascii(args)
  clear_files
  setup_files
  do_in_connection(args, current_method) { |ftp|
    ftp.chdir($ftp_work_folder)
    ftp.gettextfile($ftp_work_folder + "/" + $ascii_dest_file, $junk_file)
    check_transfer($ftp_work_folder + "/" + $ascii_dest_file, $junk_file)
  }
end

def test_download_binary(args)
  clear_files
  setup_files
  do_in_connection(args, current_method) { |ftp|
    work_folder = $ftp_work_folder
    ftp.chdir($ftp_work_folder)
    ftp.getbinaryfile($ftp_work_folder + "/" + $bin_dest_file, $junk_file)
    check_transfer($ftp_work_folder + "/" + $bin_dest_file, $junk_file)
  }
end

def check_size(ftp, full_name)
  raise "File size mismatch #{full_name}" if ftp.size(full_name) != File.size(full_name)
end

def test_file_size(args)
  clear_files
  setup_files
  do_in_connection(args, current_method) { |ftp|
    check_size(ftp, $ftp_work_folder + "/" + $create_file)
    check_size(ftp, $ftp_work_folder + "/" + $bin_dest_file)
    check_size(ftp, $ftp_work_folder + "/" + $ascii_dest_file)
  }
end

def test_rename(args)
  setup_files
  do_in_connection(args, current_method) { |ftp|
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
  }
end

def test_delete(args)
  setup_files
  do_in_connection(args, current_method) { |ftp|
    ftp.delete($ftp_work_folder + "/" + $bin_dest_file)
    check_exists($ftp_work_folder + "/" + $bin_dest_file, false)
    ftp.delete($ftp_work_folder + "/" + $ascii_dest_file)
    check_exists($ftp_work_folder + "/" + $ascii_dest_file, false)
    ftp.delete($ftp_work_folder + "/" + $create_file)
    check_exists($ftp_work_folder + "/" + $create_file, false)
  }
end

def check_exists(path, exists)
  raise "File/Path exists-#{exists} failure: #{path}" if File.exist?(path) != exists
end

def test_mkdir_rmdir(args)
  do_in_connection(args, current_method) { |ftp|
    full_mkdir_folder = $ftp_work_folder + "/" + $mkdir_folder
    ftp.mkdir(full_mkdir_folder)
    check_exists(full_mkdir_folder, true)
    ftp.rmdir(full_mkdir_folder)  
    check_exists(full_mkdir_folder, false)
  }
end

test_connect($connect_args)
test_noop($connect_args)
test_pwd($connect_args)
test_chdir($connect_args)
test_cmds_ls($connect_args)
test_upload_ascii($connect_args)
test_upload_binary($connect_args)
test_download_ascii($connect_args)
test_download_binary($connect_args)
test_file_size($connect_args)
test_rename($connect_args)
test_delete($connect_args)
test_mkdir_rmdir($connect_args)

# To write other tests for commands not supported directly through 
# the ruby interface, this method may be useful:
#    ftp.sendcmd(cmd)
