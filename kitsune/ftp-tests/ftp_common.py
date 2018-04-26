import os
import stat

connection = {'user':'anonymous',
              'passwd':'',
              'port':2021,
              'host':'localhost'}

ftp_work_folder = "/tmp/ftptest"
bin_copy_file = "/bin/ls"
ascii_copy_file = "/usr/share/dict/words"
bin_dest_file = "bin.dat"
ascii_dest_file = "asc.dat"
create_file = "empty.dat"
ftp_start_folder = "/"
junk_file = "/tmp/junk.dat"
rename_file = "rename.dat"
mkdir_folder = "test_mkdir"

def clear_files():
  file1 = ftp_work_folder + "/" + bin_dest_file
  file2 = ftp_work_folder + "/" + ascii_dest_file
  file3 = ftp_work_folder + "/" + create_file
  os.system("rm -f " + file1)
  os.system("rm -f " + file2)
  os.system("rm -f " + file3)
  os.system("rm -f " + file1 + "-ren")
  os.system("rm -f " + file2 + "-ren")
  os.system("rm -f " + file3 + "-ren")

def compare_files(path1, path2):
  0 == os.system("diff -q " + path1 + " " + path2)

def setup_files():
  os.system("touch " + ftp_work_folder + "/" + create_file)
  os.system("cp " + bin_copy_file + " " + ftp_work_folder + "/" + bin_dest_file)
  os.system("cp " + ascii_copy_file + " " + ftp_work_folder + "/" + ascii_dest_file)
  os.chmod(ftp_work_folder + "/" + bin_dest_file, stat.S_IREAD | stat.S_IWRITE | stat.S_IROTH | stat.S_IWOTH)
  os.chmod(ftp_work_folder + "/" + ascii_dest_file, stat.S_IREAD | stat.S_IWRITE | stat.S_IROTH | stat.S_IWOTH)
  
def check_transfer(file1, file2):
    if compare_files(file1, file2):
        raise "Binary File does not match."

