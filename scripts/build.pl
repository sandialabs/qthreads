#!/usr/bin/perl

use strict;
use warnings;

use Cwd qw/getcwd/;

# Setup configuration options
my %config = (
	default       => '',
	unpooled      => '--disable-pooled-memory',
	opt           => 'CFLAGS="-O3" CXXFLAGS="-O3"',
	st_shep       => '--disable-multithreaded-shepherds',
	rose          => '--enable-interfaces=rose',
	slowcontext   => '--disable-fastcontext',
	shep_profile  => '--enable-profiling=shepherd',
	lock_profile  => '--enable-profiling=lock',
	steal_profile => '--enable-profiling=steal',
	tc_profile    => '--enable-profiling=threadc',
	hi_st         => '--disable-hardware-increments --disable-multithreaded-shepherds',
	hi_mt         => '--disable-hardware-increments',
	dev           => 'CFLAGS="-g -O0" CXXFLAGS="-g -O0" --enable-debug --enable-guard-pages --enable-asserts --enable-static --disable-shared --enable-valgrind --disable-pooled-memory --enable-aligncheck'
);

my @summaries;

# Collect command-line options
my @conf_names;
my @user_configs;
my $qt_src_dir = '';
my $qt_bld_dir = '';
my $repeat = 1;
my $make_flags = '';
my $force_configure = 0;
my $force_clean = 0;
my $print_info = 0;
my $dry_run = 0;
my $need_help = 0;

if (scalar @ARGV == 0) {
    $need_help = 1;
} else {
    while (@ARGV) {
        my $flag = shift @ARGV;
    
        if ($flag =~ m/--configs=(.*)/) {
            @conf_names = split(/,/, $1);
        } elsif ($flag =~ m/--with-config=(.*)/) {
            push @user_configs, $1;
        } elsif ($flag =~ m/--source-dir=(.*)/) {
            $qt_src_dir = $1;
        } elsif ($flag =~ m/--build-dir=(.*)/) {
            $qt_bld_dir = $1;
        } elsif ($flag =~ m/--repeat=(.*)/) {
            $repeat = int($1);
        } elsif ($flag =~ m/--make-flags=(.*)/) {
            $make_flags = $1;
        } elsif ($flag eq '--force-configure') {
            $force_configure = 1;
        } elsif ($flag eq '--force-clean') {
            $force_clean = 1;
        } elsif ($flag eq '--verbose' || $flag eq '-v') {
            $print_info = 1;
        } elsif ($flag eq '--dry-run') {
            $dry_run = 1;
        } elsif ($flag eq '--help' || $flag eq '-h') {
            $need_help = 1;
        } else {
            print "Unsupported option '$flag'.\n";
            exit(1);
        }
    }
}

# Aggregate configuration options
while (@user_configs) {
    my $user_config = pop @user_configs;
    my $id = scalar @user_configs;
    my $name = "userConfig$id";

    push @conf_names, $name;
    $config{$name} = $user_config;
}
if (scalar @conf_names == 0) { push @conf_names, 'default' };
@conf_names = sort @conf_names;

if ($need_help) {
    print "usage: perl build.pl [options]\n";
    print "Options:\n";
    print "\t--configs=<config-name> comma-separated list of configurations.\n";
	print "\t                        'all' may be used as an alias for all known\n";
	print "\t                        configurations.\n";
    print "\t--with-config=<string>  a user-specified string of configuration\n";
    print "\t                        options. Essentially, this is used to define\n";
	print "\t                        an unnamed 'config', whereas the previous\n";
	print "\t                        uses pre-defined, named configs. This option\n";
	print "\t                        can be used multiple times.\n";
    print "\t--source-dir=<dir>      absolute path to Qthreads source.\n";
    print "\t--build-dir=<dir>       absolute path to target build directory.\n";
    print "\t--repeat=<n>            run `make check` <n> times per configuration.\n";
    print "\t--make-flags=<options>  options to pass to make (e.g. '-j 4').\n";
    print "\t--force-configure       run `configure` again.\n";
    print "\t--force-clean           run `make clean` before rebuilding.\n";
    print "\t--verbose\n";
    print "\t--dry-run\n";
    print "\t--help\n";

    print "Configurations:\n";
    my @names = sort keys %config;
    for my $name (@names) {
        print "\t$name:\n\t\t'$config{$name}'\n";
    }

    exit(1);
}

# Clean up and sanity check script options
my $use_all = 0;
foreach my $name (@conf_names) {
    if ($name eq 'all') {
        $use_all = 1;
    } elsif (not exists $config{$name}) {
        print "Invalid configuration option '$name'\n";
        exit(1);
    }
}
if ($use_all) {
    @conf_names = keys %config;
}

if ($qt_src_dir eq '') {
    $qt_src_dir = getcwd;
    if ((not -e "$qt_src_dir/README") || 
        (my_system("grep -q 'QTHREADS!' $qt_src_dir/README") != 0)) {
        print "Could not find the source directory; try using --source-dir.\n";
        exit(1);
    }
} elsif (not $qt_src_dir =~ m/^\//) {
    print "Specify full path for source dir '$qt_src_dir'\n";
    exit(1);
}

if ($qt_bld_dir eq '') {
    $qt_bld_dir = "$qt_src_dir/build";
} elsif (not $qt_bld_dir =~ m/^\//) {
    print "Specify full path for build dir '$qt_bld_dir'\n";
    exit(1);
}

# Optionally print information about the configuration
if ($print_info) {
    print "Configurations:   @conf_names\n";
    print "Source directory: $qt_src_dir\n";
    print "Build directory:  $qt_bld_dir\n";
}

# Run the test configurations
foreach my $conf_name (@conf_names) {
    run_tests($conf_name);
}

# Print a summary report
print "\n" . '=' x 50;
print "\nSummary:\n";
foreach my $summary (@summaries) {
    print "$summary\n";
}
print '=' x 50 . "\n";

exit(0);

################################################################################

sub run_tests {
    my $conf_name = $_[0];
    my $test_dir = "$qt_bld_dir/$conf_name";

    print "\n### Test: $conf_name\n";
    print "### Build directory: $test_dir\n";

    # Setup for configuration
    if (not -e "$qt_src_dir/configure") {
        print "###\tGenerating configure script ...\n" if ($print_info);
        my_system("cd $qt_src_dir && sh ./autogen.sh");
    }
    
    # Setup build space
    print "###\tConfiguring '$conf_name' ...\n";
    my $configure_log = "$test_dir/build.configure.log";
    my_system("mkdir -p $test_dir") if (not -e $test_dir);
    my_system("cd $test_dir && $qt_src_dir/configure $config{$conf_name} 2>&1 | tee $configure_log")
        if ($force_configure || not -e "$test_dir/config.log");
    print "### Log: $configure_log\n";
    
    # Build testsuite
    my $pass = 0;
    while ($pass < $repeat) {
        print "###\tBuilding and testing '$conf_name' pass $pass ...\n";
        my $results_log = "$test_dir/build.$pass.results.log";
        my $build_command = "cd $test_dir";
        $build_command .= " && make clean > /dev/null" if ($force_clean);
        $build_command .= " && make $make_flags check 2>&1 | tee $results_log";
        my_system($build_command);
        if (not $dry_run) {
            print "### Log: $results_log\n";
            my $build_warnings = qx/awk '\/warning:\/' $results_log/;
            if (length $build_warnings > 0) {
                print "Build warnings! Check log and/or run again with --force-clean and --verbose for more information.\n";
                print $build_warnings;
            }
            my $build_errors = qx/awk '\/error:\/' $results_log/;
            if (length $build_errors > 0) {
                print "Build error in config $conf_name! Check log and/or run again with --verbose for more information.\n";
                print $build_errors;
                exit(1);
            }

            # Display filtered results
            print "### Results for '$conf_name'\n";
            my $digest = qx/grep 'tests passed' $results_log/;
            if ($digest eq '') {
                $digest = qx/grep 'tests failed' $results_log/; chomp($digest);
                my $fails = qx/cat $results_log | awk '\/FAIL\/{print \$2}'/;
                $digest .= " (" . join(',', split(/\n/, $fails)) . ")";
            }
            chomp $digest;
            my $banner = '=' x 50;
            print "$banner\n$digest\n$banner\n";

            push @summaries, "$digest $conf_name (pass $pass)";
        }

        $pass++;
    }
}

sub my_system {
    my $command = $_[0];

    $command .= " > /dev/null" if (not $print_info);
    print "\t\$ $command\n" if ($print_info);

    my $status = system($command) if (not $dry_run);

    return $status;
}

