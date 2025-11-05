"""
record_builds.py records time-series build status data for workflows like Picolibc, MUSL.
This script generates data that is consumed by a server-less web-app to show
a dashboard with summarized, workflow-wise and historic build pass/fail information.

Usage:
record_builds.py tool is invoked during the github workflow process.
The build status is marked as "Pass" if both the compilation and testing
completes without error or failures.
E.g.
    Record: python record_builds.py --workflow musl --record --pass --run_id 1500
    Update: python record_builds.py --workflow musl --update --run_id 1650 --pass
    Emit: python record_builds.py --workflow musl --emit
This tool needs no user invocation or intervention for generating data and displaying the dashboard.
"""

from argparse import ArgumentParser
from datetime import date, datetime
import json
import sys
import atexit

try:
    import sqlite3
except Exception as e:
    sys.exit("Unable to import sqlite3: " + str(e))

# Global
_build_status_db = "build-status.db"
_build_status_db_connection = None


def close_connection():
    global _build_status_db_connection
    if _build_status_db_connection:
        _build_status_db_connection.close()
        _build_status_db_connection = None


def get_connection():
    global _build_status_db_connection
    if not _build_status_db_connection:
        _build_status_db_connection = sqlite3.connect(_build_status_db)
    return _build_status_db_connection


def createBuildDataTables(workflow):
    # Create table for a workflow
    conn = get_connection()
    cursor = conn.cursor()
    create_table = ""
    # Table has build_count as first column to allow entries with same run_id value.
    # Same run_id values are recorded in case of parallel builds.
    # Set unique constraint on run_id and architecture.
    create_table = (
        " CREATE TABLE IF NOT EXISTS "
        + workflow
        + " (build_count INTEGER PRIMARY KEY AUTOINCREMENT, run_id INTEGER, state TEXT, build_date TEXT, build_time TEXT, arch TEXT, UNIQUE(run_id, arch));"
    )
    try:
        cursor.execute(create_table)
    except Exception as e:
        print(f"Error creating table '" + workflow + "': {e}")
    finally:
        conn.commit()


def addNewBuildData(args):
    # Insert data in workflow specific tables
    workflow_table = args.workflow_build.lower()
    conn = get_connection()
    cursor = conn.cursor()
    try:
        cursor.execute(
            "INSERT INTO "
            + workflow_table
            + " (run_id, state, build_date, build_time, arch) VALUES (?, ?, ?, ?, ?)",
            (
                args.run_id,
                "pass" if args.build_status else "fail",
                str(date.today()),
                datetime.now().strftime("%H:%M"),
                args.workflow_arch,
            ),
        )
    except Exception as e:
        sys.exit(
            "Error adding new time-series build status data to "
            + workflow_table
            + ": "
            + str(e)
        )
    finally:
        conn.commit()


def recordBuildData(args):
    # Create tables if not available.
    createBuildDataTables(args.workflow_build.lower())
    # Insert data in tables
    addNewBuildData(args)


def updateBuildData(args):
    # Update state of existing build status data.
    conn = get_connection()
    cursor = conn.cursor()
    workflow_table = args.workflow_build.lower()
    try:
        if workflow_table == "picolibc":
            cursor.execute(
                "UPDATE "
                + workflow_table
                + " SET state = ? WHERE run_id = ? AND arch = ?",
                (
                    "pass" if args.build_status else "fail",
                    args.run_id,
                    args.workflow_arch,
                ),
            )
        else:
            cursor.execute(
                "UPDATE " + workflow_table + " SET state = ? WHERE run_id = ?",
                ("pass" if args.build_status else "fail", args.run_id),
            )
    except Exception as e:
        sys.exit("Error updating build status data in " + workflow_table + ":" + str(e))
    finally:
        conn.commit()


def handleBuildData(args):
    # Check arguments to either insert new build status data or update existing.
    if args.record_mode:
        # Record data in db
        recordBuildData(args)
        print("\nBuild data recorded to table " + args.workflow_build.lower() + "\n")
    elif args.update_mode:
        # Update status of existing build data.
        updateBuildData(args)
        print("\nBuild data updated in table " + args.workflow_build.lower() + "\n")


def writeJSData(workflow, data):
    # Emit build status data from DB to workflow specific file allowing faster
    # loads in a server-less design.

    # The emitted file extension and data is saved in form of a JavaScript compatible variable
    # that allows non-module JavaScript auto-import, without exporting JS variables
    # and without getting blocked by browser's CORS policy.

    workflow_file_name = workflow + "_status_data.py"
    workflow_javascript_var_assignment = workflow + "_build_states = "
    try:
        # Add JavaScript variable assignment.
        with open(workflow_file_name, "w") as file:
            file.write(workflow_javascript_var_assignment)
        # Emit updated time-series build status data.
        with open(workflow_file_name, "a") as file:
            json.dump(data, file)
    except Exception as e:
        sys.exit("Unable to write build data: " + str(e))
    print("\nBuild data written to " + workflow_file_name + "\n")


def emitJSData(args):
    workflow = args.workflow_build.lower()
    conn = get_connection()
    cursor = conn.cursor()
    all_data = []
    all_states_data = []
    try:
        if args.workflow_build.lower() == "picolibc":
            cursor.execute(
                "SELECT run_id, state, build_date, build_time, arch FROM "
                + workflow
                + ";"
            )
        else:
            cursor.execute(
                "SELECT run_id, state, build_date, build_time FROM " + workflow + ";"
            )
        all_data = cursor.fetchall()
    except Exception as e:
        print(
            "Error fetching build status data from table " + workflow + ": " + str(e)
        )

    for data in all_data:
        if args.workflow_build.lower() == "picolibc":
            new_state = {
                "run_id": str(data[0]),
                "state": data[1],
                "date": data[2],
                "time": data[3],
                "arch": data[4],
            }
        else:
            new_state = {
                "run_id": str(data[0]),
                "state": data[1],
                "date": data[2],
                "time": data[3],
            }
        all_states_data.append(new_state)

    # Emit JS file.
    writeJSData(workflow, all_states_data)


def validateArgs(args):
    # Validate workflow
    if args.workflow_build.lower() not in ["picolibc", "musl"]:
        sys.exit("Unsupported Workflow!")

    # Record/update mode must have a run_id.
    if args.record_mode or args.update_mode:
        if args.run_id is None:
            sys.exit(
                "Record/update mode must have a build run_id! E.g. --run-id <run_id>"
            )


def handleArguments():
    parser = ArgumentParser(
        description="A program that records time-series build status data."
    )
    parser.add_argument(
        "--workflow",
        "-w",
        dest="workflow_build",
        required=True,
        help="The workflow build name. Supported workflows: picolibc and musl",
    )
    parser.add_argument(
        "--arch",
        "-a",
        dest="workflow_arch",
        required=False,
        help="The workflow build architecture name.",
    )
    parser.add_argument(
        "--run-id", dest="run_id", required=False, help="The github workflow run ID."
    )
    # Create a build status record or update flag group.
    record_mode_group = parser.add_mutually_exclusive_group(required=True)
    record_mode_group.add_argument(
        "--record",
        "-r",
        dest="record_mode",
        action="store_true",
        help="Record a build.",
    )
    record_mode_group.add_argument(
        "--update",
        "-u",
        dest="update_mode",
        action="store_true",
        help="Update a build.",
    )
    # Add flag to emit JavaScript compatible variable declaration file.
    record_mode_group.add_argument(
        "--emit",
        "-e",
        dest="emit_data",
        action="store_true",
        help="Emit build data in JavaScript format.",
    )
    # Create a build status flag group to make either flag required.
    status_group = parser.add_mutually_exclusive_group()
    status_group.add_argument(
        "--pass",
        "-p",
        dest="build_status",
        action="store_true",
        help="Record a passing build.",
    )
    status_group.add_argument(
        "--fail",
        "-f",
        dest="build_status",
        action="store_false",
        help="Record a failing build.",
    )
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


if __name__ == "__main__":
    main()
