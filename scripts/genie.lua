DIR = (path.getabsolute("..") .. "/")
local THIRD_DIR  = (DIR .. "3rdparty/")
local BUILD_DIR  = (DIR .. "build/")


solution "gabibits"
	configurations {
		"debug",
		"development",
		"release",
	}

	platforms {
		"x32",
		"x64",
		"native"
	}

	language "C"

	configuration {}

dofile ("toolchain.lua")
toolchain(BUILD_DIR, THIRD_DIR)

dofile("gabibits.lua")
gabibits_project("", "WindowedApp", {})
