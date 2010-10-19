#!/usr/bin/python

# /usr/share/abrt-retrace/create.wsgi

import sys
sys.path = ["/usr/share/abrt-retrace"] + sys.path

from retrace import *
from tempfile import *

def application(environ, start_response):
    read_config()

    request = Request(environ)

    if request.method != "POST":
        return response(start_response, "405 Method Not Allowed")

    if not request.content_type in ["application/x-xz", "application/x-xz-compressed-tar"]:
        return response(start_response, "415 Unsupported Media Type")

    if not request.content_length:
        return response(start_response, "411 Length Required")

    if request.content_length > CONFIG["MaxPackedSize"] * 1048576:
        return response(start_response, "413 Request Entity Too Large")

    space = free_space(CONFIG["WorkDir"])
    if not space:
        return response(start_response, "500 Internal Server Error", "Unable to obtain disk free space")

    if space - request.content_length < CONFIG["MinStorageLeft"] * 1048576:
        return response(start_response, "507 Insufficient Storage")

    try:
        archive = NamedTemporaryFile(mode = "wb", delete = False, suffix = ".tar.xz")
        archive.write(request.body)
        archive.close()
    except:
        return response(start_response, "500 Internal Server Error", "Unable to save archive")

    size = unpacked_size(archive.name)
    if not size:
        os.unlink(archive.name)
        return response(start_response, "500 Internal Server Error", "Unable to obtain unpacked size")

    if size > CONFIG["MaxUnpackedSize"] * 1048576:
        os.unlink(archive.name)
        return response(start_response, "413 Request Entity Too Large")

    if space - size < CONFIG["MinStorageLeft"] * 1048576:
        os.unlink(archive.name)
        return response(start_response, "507 Insufficient Storage")

    taskid, taskdir = new_task()
    if not taskid or not taskdir:
        return response(start_response, "500 Internal Server Error", "Unable to create new task")

    try:
        os.chdir(taskdir)
        unpack_retcode = unpack(archive.name)
        os.unlink(archive.name)

        if unpack_retcode != 0:
            raise
    except:
        os.chdir("/")
        os.system("rm -rf " + taksdir)
        return response(start_response, "500 Internal Server Error", "Unable to unpack archive")

    files = os.listdir(".")

    for required_file in REQUIRED_FILES:
        if not required_file in files:
            os.chdir("/")
            os.system("rm -rf " + taskdir)
            return response(start_response, "403 Forbidden")

    return response(start_response, "201 Created", "", [("X-Task-Id", taskid), ("X-Task-Est-Time", str(get_task_est_time(taskdir)))])