#!/usr/bin/env perl
# Copyright © 2010 Edward Smith
# 
# All rights reserved. 
# 
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice, 
# this list of conditions and the following disclaimer. 
# 
# 2. Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution. 
# 
# 3. The names of the contributors may not be used to endorse or promote 
# products derived from this software without specific prior written 
# permission. 
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
# POSSIBILITY OF SUCH DAMAGE. 
# 
use strict;
use warnings;


# Variables:
my $REPO_ROOT;
my $VSFTPD_TREE_PATH = "examples/vsftpd";
my $VSFTPD_CONF_PATH = $VSFTPD_TREE_PATH . "/" . "vsftpd.conf";
my $VSFTPD_TEST_PATH = "tests/ftp-tests";
my @VSFTPD_VERSION_LIST = qw/1.1.3 1.2.0 1.2.1 1.2.2/;
# Assigning hash{list} = {list} maps each element of the first list to
# the corresponding element in the second list.
my %VSFTPD_VERSION_INDEX;
@VSFTPD_VERSION_INDEX{@VSFTPD_VERSION_LIST[0..$#VSFTPD_VERSION_LIST]} = (0 .. $#VSFTPD_VERSION_LIST);

# Returns the path to the root of the bzr repo
sub get_repo_root {
    my $bzr_info_out = `bzr info`;
    my @bzr_out_lines = split(/\n/, $bzr_info_out);
    my $path;

    for (@bzr_out_lines) {
	if (/^  repository branch: (.*)$/) {
	    $path = $1;
	    last;
	}
    }
    return $path;
}

sub run_tests_mainloop {
    my $full_path_to_tests = $REPO_ROOT . "/" . $VSFTPD_TEST_PATH;
    my $test_script = $full_path_to_tests . "/" . "ftp-test.rb > /dev/null";
    my $test_return_value;


    system("mkdir -p /tmp/ftptest");


    $test_return_value = system("$test_script");

    return $test_return_value
}

sub initiate_update {
    my ($args_href) = (@_);
    my $target_pid = $args_href->{'pid'};
    my $signal_retval;

    $signal_retval = system("kill -USR2 \'$target_pid\'");

    return $signal_retval;
}

sub launch_vsftp {
    my ($args_href) = (@_);
    my $version = $args_href->{'version'};
    my $fork_pid;
    my $path_to_bin = "${REPO_ROOT}/" .
	"${VSFTPD_TREE_PATH}/" . 
	"vsftpd-${version}/" .
	"vsftpd";

    if(($fork_pid = fork) != 0){
	# parent
	return $fork_pid;
    }
    else {
	my $command_line = "$path_to_bin ${REPO_ROOT}/${VSFTPD_CONF_PATH}";
	exec($command_line) or print STDERR "couldn't exec $command_line: $!";
    }
}

sub kill_vsftpd {
    my ($args_href) = (@_);
    my $vsf_pid = $args_href->{'pid'};
    my $retval;

    $retval = system("kill -INT $vsf_pid");
    return $retval;
}

sub launch_client {
    my ($args_href) = @_;
    my $ftp_command = "ftp localhost 2021";
    my ($pipe_handle, $pid);

    $pid = open($pipe_handle, "|-", $ftp_command) 
	|| die ("couldn't launch $ftp_command\n") unless defined $pid;

    return {'pid' => $pid, 'pipe' => $pipe_handle} # hash ref
}

sub run_child_tests {
    my ($client_pipe) = @_;
    
    # just check that the server is still up by sending an RHELP
    print $client_pipe, "rhelp";
    
    # right now we have no way of doing anything better than this
    return 1;
}

sub run_streak {
    my ($args_href) = (@_);
    my $test_func_ref = $args_href->{'test_func_ref'};
    my $starting_version = $args_href->{'start'};
    my $pid = $args_href->{'pid'};
    my $id = $args_href->{'id'};

    my ($starting_index) = $VSFTPD_VERSION_INDEX{$starting_version};	
    my @streak_version = 
	@VSFTPD_VERSION_LIST[$starting_index .. $#VSFTPD_VERSION_LIST];

    for(@streak_version) {
	if (&$test_func_ref == 0) {
	    print "Pre-update  $id: version $_ passed\n";
	} else {
	    print "Pre-update  $id: version $_ failed\n";
	}
	sleep(1);
	initiate_update({'pid' => $pid});
	sleep(1);
	if (&$test_func_ref == 0) {
	    print "Post-update $id: version $_ passed\n";
	} else {
	    print "Post-update $id: version $_ failed\n";
	}	
    }
}

# main
$REPO_ROOT = get_repo_root;

my ($version, $test_return);
for $version(@VSFTPD_VERSION_LIST) {
    # launch the version
    my $vsftp_pid = launch_vsftp({'version' => "$version"});
    
    # get a client
    # my $client_ret_href = launch_client;
    # my $client_pid = $client_ret_href->{'pid'};
    # my $client_pipe = $client_ret_href->{'pipe'};

    # test the client loop from version onward
    # run_streak({'pid' => $client_pid, 'id' => "clientloop from $version",
    # 		'start' => $version, 
    # 		'test_func_ref' => sub {return run_child_tests($client_pipe)}})


    # test the mainloop from version onward
    run_streak({'pid' => $vsftp_pid, 'id' => "mainloop from $version", 
    		'start' => $version, 'test_func_ref' => \&run_tests_mainloop});

    # kill the version
    kill_vsftpd({'pid' => $vsftp_pid});
    die "Got unknown PID from wait!" if (wait() != $vsftp_pid);
}


exit(0);
    
    

