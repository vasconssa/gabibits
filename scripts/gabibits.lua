--
-- Copyright (c) 2012-2020 Daniele Bartolini and individual contributors.
-- License: https://github.com/dbartolini/crown/blob/master/LICENSE
--

function gabibits_project(_name, _kind, _defines)

	project ("gabibits" .. _name)
		kind (_kind)

		includedirs {
			DIR .. "include",
			DIR .. "3rdparty",
		}

		defines {
			_defines,
		}

		links {
		}

		configuration { "debug or development" }
			defines {
				"GB_DEBUG=1"
			}

		configuration { "development" }
			defines {
				"GB_DEVELOPMENT=1"
			}

		configuration { "android*" }
			kind "ConsoleApp"
			targetextension ".so"
			linkoptions {
				"-shared"
			}
			links {
				"gcc",
				"EGL",
				"GLESv2",
				"OpenSLES",
			}

		configuration { "linux-*" }
            defines {
                "GB_USE_XCB",
            }
			links {
                "xcb-util",
                "xcb-ewmh",
                "xcb-keysyms",
                "xcb",
				"pthread",
                "m",
                "dl",
			}

		configuration { "vs* or mingw*" }
			links {
				"dbghelp",
				"xinput",
				"psapi",
				"ws2_32",
				"ole32",
				"gdi32",
			}

		configuration {}

		files {
			DIR .. "src/**.c",
		}

        configuration { "android-arm" }
        files {
                DIR .. "src/sx/asm/make_arm_aapcs_elf_gas.S",
                DIR .. "src/sx/asm/jump_arm_aapcs_elf_gas.S",
                DIR .. "src/sx/asm/ontop_arm_aapcs_elf_gas.S",
        }

        configuration { "x32", "linux*" }
        files {
                DIR .. "src/sx/asm/make_i386_sysv_elf_gas.S",
                DIR .. "src/sx/asm/jump_i386_sysv_elf_gas.S",
                DIR .. "src/sx/asm/ontop_i386_sysv_elf_gas.S",
        }

        configuration { "x64", "linux*" }
        files {
                DIR .. "src/sx/asm/make_x86_64_sysv_elf_gas.S",
                DIR .. "src/sx/asm/jump_x86_64_sysv_elf_gas.S",
                DIR .. "src/sx/asm/ontop_x86_64_sysv_elf_gas.S",
        }

        configuration { "x32", "mingw*", "vs*" }
        files {
                DIR .. "src/sx/asm/make_i386_ms_pe_masm.asm",
                DIR .. "src/sx/asm/jump_i386_ms_pe_masm.asm",
                DIR .. "src/sx/asm/ontop_i386_ms_pe_masm.asm",
        }

        configuration { "x64", "mingw*", "vs*" }
        files {
                DIR .. "src/sx/asm/make_x86_64_ms_pe_masm.asm",
                DIR .. "src/sx/asm/jump_x86_64_ms_pe_masm.asm",
                DIR .. "src/sx/asm/ontop_x86_64_ms_pe_masm.asm",
        }

		strip()

		configuration {} -- reset configuration
end
