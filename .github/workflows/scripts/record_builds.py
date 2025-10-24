from argparse import ArgumentParser
from datetime import date, datetime
import json
import sys
import atexit

try:
	import sqlite3
except Exception as e:
	sys.exit("Unable to import sqlite3 build data: " + e)

# Global
build_status_db = 'build-status.db'
_build_status_db_connection = None

def close_connection():
	global _build_status_db_connection
	if _build_status_db_connection:
		_build_status_db_connection.close()
		_build_status_db_connection = None


def get_connection():
    global _build_status_db_connection
    if not _build_status_db_connection:
    	_build_status_db_connection = sqlite3.connect(build_status_db)
    return _build_status_db_connection

def createBuildDataTables(target):
	# Create table for a target
	conn = get_connection()
	cursor = conn.cursor()
	create_table = " CREATE TABLE IF NOT EXISTS "+ target +" (id INTEGER PRIMARY KEY UNIQUE, state TEXT, build_date TEXT, build_time TEXT);"
	try:
	    cursor.execute(create_table)
	except Exception as e:
	    print(f"Error creating table '"+ target +"': {e}")
	finally:
	    conn.commit()

def addNewBuildData(args):
	# Insert data in target specific tables
	target_table = args.target_build.lower()
	conn = get_connection()
	cursor = conn.cursor()
	try:
		cursor.execute("INSERT INTO "+ target_table +" (id, state, build_date, build_time) VALUES (?, ?, ?, ?)", 
                   (args.build_id, 'pass' if args.build_status else 'fail', str(date.today()), datetime.now().strftime("%H:%M")))
	except Exception as e:
		sys.exit("Error adding new time-series build status data to "+ target_table + ": " + e)
	finally:
	    conn.commit()

def recordBuildData(args):
	# Create tables if not available.
	createBuildDataTables(args.target_build.lower())
	# Insert data in tables
	addNewBuildData(args)

def updateBuildData(args):
	# Update state of existing build status data.
	conn = get_connection()
	cursor = conn.cursor()
	try:
		cursor.execute("UPDATE "+args.target_build.lower()+" SET state = ? WHERE id = ?",('pass' if args.build_status else 'fail', args.build_id))
	except Exception as e:
		sys.exit("Error updating build status data in "+ target_table + ":" + e)
	finally:
		conn.commit()


def handleBuildData(args):
	# Check arguments to either insert new build status data or update existing.
	if args.record_mode:
		# Record data in db
		recordBuildData(args)
	elif args.update_mode:
		# Update status of existing build data.
		updateBuildData(args)
	print("\nBuild data recorded to table " + args.target_build.lower() + "\n")

def writeJSData(target, data):
	# Emit build status data from DB to target specific file.

	# Data is saved in form of a JavaScript compatible variable
	# that allows non-module JavaScript auto-import, without exporting JS variables
	# and without getting blocked by browser's CORS policy.

	target_file_name = target + "_status_data.py"
	target_javascript_var_assignment = target + "_build_states = "
	try:
		# Add JavaScript variable assignment.
		with open(target_file_name, "w") as file: file.write(target_javascript_var_assignment)
		# Emit updated time-series build status data.
		with open(target_file_name, "a") as file: json.dump(data, file)
	except Exception as e:
		sys.exit("Unable to write build data: " + e)
	print("\nBuild data emitted to " + target_file_name + "\n")

def emitJSData(args):
	target = args.target_build.lower()
	conn = get_connection()
	cursor = conn.cursor()
	all_data = None
	all_states_data = []
	try:
		cursor.execute("SELECT * FROM "+ target +";")
		all_data = cursor.fetchall()
	except Exception as e:
		sys.exit("Error fetching build status data from table "+target+": " + e)

	for data in all_data:
		new_state = {"id": str(data[0]), "state": data[1], "date": data[2], "time": data[3]}
		all_states_data.append(new_state)

	# Emit JS file.
	writeJSData(target, all_states_data)

def validateArgs(args):
	# Validate target
	if args.target_build.lower() == 'picolibc' or args.target_build.lower() == 'musl' or args.target_build.lower() == 'zephyr':
		pass
	else:
		sys.exit("Unsupported Target!")

	# Record/update mode must have a build status flag
	if args.record_mode or args.update_mode:
		if args.build_id is None:
			sys.exit("Record/update mode must have a build ID! E.g. --id <build-id>")

def handleArguments():
	parser = ArgumentParser(description="A program that records time-series build status data.")
	parser.add_argument("--target", "-t", dest="target_build", required=True,
                        help="The target build name. Supported targets: picolibc, musl, zephyr")
	parser.add_argument("--id", dest="build_id", required=False,
                        help="The unique build run ID.")
	# Create a build status record or update flag group.
	record_mode_group = parser.add_mutually_exclusive_group(required=True)
	record_mode_group.add_argument("--record", "-r", dest="record_mode", action='store_true',
                        help="Record a build.")
	record_mode_group.add_argument("--update", "-u", dest="update_mode", action='store_true',
                        help="Update a build.")
	# Add flag to emit JavaScript compatible variable declaration file.
	record_mode_group.add_argument("--emit", "-e", dest="emit_data", action='store_true',
                        help="Emit build data in JavaScript format.")
	# Create a build status flag group to make either flag required.
	status_group = parser.add_mutually_exclusive_group()
	status_group.add_argument("--pass", "-p", dest="build_status", action='store_true',
                        help="Record a passing build.")
	status_group.add_argument("--fail", "-f", dest="build_status", action='store_false',
                        help="Record a failing build.")
	args = parser.parse_args()

	# Validate arguments
	validateArgs(args)
	return args

def main():
	# Handle arguments.
	args = handleArguments()

	if args.emit_data:
		# For dashboard, emit data in JavaScript compatible variable declaration file.
		emitJSData(args)
	else:
		# Insert or update build state data
		handleBuildData(args)

	# Cleanup
	atexit.register(close_connection)

'''
record_builds.py records time-series build status data for Picolibc, MUSL and
Zephyr builds.
This script generates data that is consumed by a server-less web-app to show
a dashboard with summarized, target-wise and historic build pass/fail information.

Usage:
record_builds.py is tool invoked by the GitHub workflow during the target build process.
The build status is marked as "Pass" if both the compilation and testing
completes without error or failures.
E.g.
    Record: python record_builds.py --target musl --record --pass --id 1500
    Update: python record_builds.py --target musl --update --id 1650 --pass
    Emit: python record_builds.py --target musl --emit
This tool needs no user invocation or intervention in generating data and displaying the dashboard.
'''
if __name__ == '__main__':
	main()