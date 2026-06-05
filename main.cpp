/* lfs-pkgman - simple package manager for Linux From Scratch
Installs packages compile and installed into a directory with a command like "make DESTDIR=package_destdir install"
Also checks conflicts with other tracked packages and preserved manually marked config files
*/

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <unordered_map>
#include <unordered_set>
using namespace std;
namespace fs = std::filesystem;

// Change this variable and recompile to install packages into another root
const fs::path root = "/";

vector<fs::path> contents;
unordered_map<fs::path, string> owner;

void show_help() {
	cout << R"(lfs-pkgman - simple package manager for Linux From Scratch

Usage:
    lfs-pkgman <command> [arguments]

Commands:
    help
        Show this help message.
    install <name> <version> <directory>
        Install package from given directory into root and add it to package list with given name and version.
        For packages from the LFS books, it's recommended that you use the same name and version from the book.
    uninstall <name>
        Uninstall package with given name.
    mark-config <file>
        Mark the given file as a configuration file, so that it doesn't get overwritten or removed by this package manager.
    unmark-config <file>
        Remove the given file from the config file list, allowing it to be overwritten/deleted by package operations.
        Doesn't delete the file itself.)" << '\n';
}

// Recursively gets all files/symlinks from a given directory
int get_contents(fs::path current_directory) {
	int error_code = 0;
	try {
		for (auto const& entry:fs::directory_iterator{current_directory}) {
			if (entry.is_directory() && !entry.is_symlink()) {
				if (get_contents(entry) == 1) {
					error_code = 1;
				}
			}
			else contents.push_back(entry.path());
		}
	}
	catch (fs::filesystem_error e) {
		cerr << "lfs-pkgman install: " << e.code().message() << '\n';
		error_code = 1;
	}
	return error_code;
}

void convert_to_lowercase(string &s) {
	for (auto &ch:s) {
		if (isalpha(ch)) {
			ch = tolower(ch);
		}
	}
}

// Removes a file, while also removing unnecessary parent directories recursively
void remove_with_cleanup(fs::path path, error_code &ec) {
	if (path == root) return;
	fs::remove(path, ec);
	if (ec) return;
	remove_with_cleanup(path.parent_path(), ec);
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cerr << "lfs-pkgman: no command given\n";
		cerr << "Showing help:\n\n";
		show_help();
		return 1;
	}
	string command = argv[1];
	if (command == "help") {
		show_help();
		return 0;
	}
	// install command - installs a package
	else if (command == "install") {
		if (argc != 5) {
			if (argc < 5) {
				cerr << "lfs-pkgman install: not enough arguments";
			}
			else {
				cerr << "lfs-pkgman install: too many arguments";
			}
			cerr << R"(

Usage:
    install <name> <version> <directory>)" << '\n';
			return 1;
		}
		string name = argv[2];
		convert_to_lowercase(name);
		string version = argv[3];
		string pkg_dir_name = argv[4];
		fs::directory_entry pkg_dir{pkg_dir_name};
		if (!pkg_dir.exists()) {
			cerr << "lfs-pkgman install: '" << pkg_dir_name << "': no such file or directory\n";
			return 1;
		}
		if (!pkg_dir.is_directory()) {
			cerr << "lfs-pkgman install: '" << pkg_dir_name << "': not a directory\n";
			return 1;
		}
		cout << "Getting package contents...\n";
		if (get_contents(pkg_dir.path()) == 1) {
			return 1;
		}
		for (auto &entry:contents) {
			entry = fs::relative(entry, pkg_dir.path());
		}
		try {
			fs::path metadata_dir_path = root / "var/lib/lfs-pkgman/packages/";
			fs::create_directories(metadata_dir_path);
			fs::path metadata_file_path = metadata_dir_path / name;
			cout << "Checking for conflicts with other packages...\n";
			for (auto entry:fs::directory_iterator{metadata_dir_path}) {
				if (entry.path() == metadata_file_path) continue;
				ifstream current_metadata_file{entry.path()};
				if (!current_metadata_file) {
					cerr << "lfs-pkgman install: failed to open metadata file: " << entry.path().string() << '\n';
					return 1;
				}
				string other_version;
				string current_file_path_string;
				fs::path current_file_path;
				getline(current_metadata_file, other_version);
				while (getline(current_metadata_file, current_file_path_string)) {
					if (current_file_path_string.empty()) continue;
					current_file_path = current_file_path_string;
					owner[current_file_path] = entry.path().filename().string();
				}
			}
			for (auto entry:contents) {
				if (owner.find(entry) != owner.end()) {
					cerr << "lfs-pkgman install: file " << entry << " conflicts with package " << owner[entry] << '\n';
					return 1;
				}
			}
			unordered_set<fs::path> config_files;
			fs::path config_file_list_path = root / "var/lib/lfs-pkgman/config_files";
			if (fs::exists(config_file_list_path)) {
				ifstream config_file_list_in(config_file_list_path);
				if (!config_file_list_in) {
					cerr << "lfs-pkgman install: error opening config files list for reading\n";
					return 1;
				}
				string current_file_path;
				while (getline(config_file_list_in, current_file_path)) {
					if (current_file_path.empty()) continue;
					config_files.insert(fs::path(current_file_path));
				}
			}
			cout << "Copying files...\n";
			for (auto entry:contents) {
				if (config_files.count(entry) == 0) {
					fs::path destination = root / entry;
					fs::create_directories((destination).parent_path());
					if (fs::exists(destination)) {
						fs::remove_all(destination);
					}
					fs::copy(pkg_dir / entry, destination, fs::copy_options::overwrite_existing | fs::copy_options::copy_symlinks);
				}
			}
			if (fs::exists(metadata_file_path)) {
				cout << "Package was reinstalled, cleaning up previous version...\n";
				unordered_set<fs::path> new_files(contents.begin(), contents.end());
				ifstream metadata_file{metadata_file_path};
				if (!metadata_file) {
					cerr << "lfs-pkgman install: failed to open metadata file: " << metadata_file_path.string() << '\n';
					return 1;
				}
				string old_version;
				string entry;
				fs::path entry_path;
				getline(metadata_file, old_version);
				while (getline(metadata_file, entry)) {
					if (entry.empty()) continue;
					entry_path = entry;
					if (new_files.count(entry_path) == 0 && config_files.count(entry_path) == 0) {
						error_code ec;
						remove_with_cleanup(root / entry_path, ec);
						if (ec) {
							if (ec == errc::no_such_file_or_directory) {
								continue;
							}
							if (ec == errc::directory_not_empty) {
								continue;
							}
							cerr << "Failed to remove " << entry << ": " << ec.message() << '\n';
						}
					}
				}
			}
			cout << "Writing package metadata file...\n";
			ofstream metadata_file{metadata_file_path};
			if (!metadata_file) {
				cerr << "lfs-pkgman install: failed to open metadata file: " << metadata_file_path.string() << '\n';
				return 1;
			}
			metadata_file << version << '\n';
			for (auto entry:contents) metadata_file << entry.string() << '\n';
			cout << "Successfully installed " << name << '\n';
		}
		catch (fs::filesystem_error e) {
			cerr << "lfs-pkgman install: " << e.code().message() << '\n';
			return 1;
		}
	}
	// uninstall command - uninstalls a package
	else if (command == "uninstall") {
		if (argc != 3) {
			if (argc < 3) {
				cerr << "lfs-pkgman uninstall: not enough arguments";
			}
			else {
				cerr << "lfs-pkgman uninstall: too many arguments";
			}
			cerr << R"(

Usage:
    uninstall <name>)" << '\n';
			return 1;
		}
		string name = argv[2];
		convert_to_lowercase(name);
		if (!exists(root / "var/lib/lfs-pkgman/packages" / name)) {
			cerr << "lfs-pkgman uninstall: package " << name << " is not installed\n";
			return 1;
		}
		fs::path metadata_file_path = root / "var/lib/lfs-pkgman/packages/" / name;
		ifstream metadata_file{metadata_file_path};
		if (!metadata_file) {
			cerr << "lfs-pkgman uninstall: failed to open metadata file: " << metadata_file_path.string() << '\n';
			return 1;
		}
		string version;
		string entry;
		fs::path entry_path;
		getline(metadata_file, version);
		cout << "Uninstalling " << name << "...\n";
		while (getline(metadata_file, entry)) {
			if (entry.empty()) continue;
			entry_path = entry;
			error_code ec;
			remove_with_cleanup(root / entry_path, ec);
			if (ec) {
				if (ec == errc::no_such_file_or_directory) {
					continue;
				}
				if (ec == errc::directory_not_empty) {
					continue;
				}
				cerr << "Failed to remove " << entry << ": " << ec.message() << '\n';
			}
		}
		error_code ec;
		fs::remove(metadata_file_path, ec);
		if (ec) {
			cerr << "Failed to remove metadata file: " << ec.message() << '\n';
		}
		cout << "Successfully uninstalled " << name << '\n';
	}
	// mark-config command - marks file as configuration file so it doesn't get overwritten by reinstall/uninstall
	else if (command == "mark-config") {
		if (argc != 3) {
			if (argc < 3) {
				cerr << "lfs-pkgman mark-config: not enough arguments";
			}
			else {
				cerr << "lfs-pkgman mark-config: too many arguments";
			}
			cerr << R"(

Usage:
    mark-config <file>)" << '\n';
			return 1;
		}
		fs::path config_file_path = argv[2];
		if (!fs::exists(config_file_path)) {
			cerr << "lfs-pkgman mark-config: " << config_file_path.string() << ": no such file or directory\n";
			return 1;
		}
		if (!fs::is_regular_file(config_file_path) && !fs::is_symlink(config_file_path)) {
			cerr << "lfs-pkgman mark-config: " << config_file_path.string() << ": not a file or symlink\n";
			return 1;
		}
		fs::path config_file_list_path = root / "var/lib/lfs-pkgman/config_files";
		if (fs::exists(config_file_list_path)) {
			ifstream config_file_list_in(config_file_list_path);
			if (!config_file_list_in) {
				cerr << "lfs-pkgman mark-config: error opening config files list for reading\n";
				return 1;
			}
			string current_config_file_path_string;
			fs::path current_config_file_path;
			while (getline(config_file_list_in, current_config_file_path_string)) {
				if (current_config_file_path_string.empty()) continue;
				current_config_file_path = current_config_file_path_string;
				if (current_config_file_path == fs::relative(config_file_path, root)) {
					cout << "File is already marked as config\n";
					return 0;
				}
			}
		}
		ofstream config_file_list_out(config_file_list_path, ios::app);
		if (!config_file_list_out) {
			cerr << "lfs-pkgman mark-config: error opening config files list for writing\n";
			return 1;
		}
		config_file_list_out << fs::relative(config_file_path, root).string() << '\n';
	}
	// unmark-config command - unmarks a configuration file
	else if (command == "unmark-config") {
		if (argc != 3) {
			if (argc < 3) {
				cerr << "lfs-pkgman unmark-config: not enough arguments";
			}
			else {
				cerr << "lfs-pkgman unmark-config: too many arguments";
			}
			cerr << R"(

Usage:
    unmark-config <file>)" << '\n';
			return 1;
		}
		fs::path config_file_path = argv[2];
		fs::path config_file_list_path = root / "var/lib/lfs-pkgman/config_files";
		if (!fs::exists(config_file_list_path)) {
			cout << "Config file list doesn't exist, nothing changed\n";
			return 0;
		}
		ifstream config_file_list_in(config_file_list_path);
		if (!config_file_list_in) {
			cerr << "lfs-pkgman unmark-config: error opening config file list for reading\n";
			return 1;
		}
		vector<string> config_files;
		string current_config_file_string;
		fs::path current_config_file;
		bool exists = false;
		while (getline(config_file_list_in, current_config_file_string)) {
			if (current_config_file_string.empty()) continue;
			current_config_file = current_config_file_string;
			if (current_config_file != fs::relative(config_file_path, root)) {
				config_files.push_back(current_config_file_string);
			}
			else {
				exists = true;
			}
		}
		if (!exists) {
			cout << "File isn't marked as config, nothing changed\n";
			return 0;
		}
		ofstream config_file_list_out(config_file_list_path);
		if (!config_file_list_out) {
			cerr << "lfs-pkgman unmark-config: error opening config file list for writing\n";
			return 1;
		}
		for (auto config_file:config_files) {
			config_file_list_out << config_file << '\n';
		}
	}
	else {
		cerr << "lfs-pkgman: unknown command '" << command << "'\n";
	}
}
