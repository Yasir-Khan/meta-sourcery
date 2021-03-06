#
# Configuration to use external Sourcery G++ toolchain
#

EXTERNAL_TOOLCHAIN ?= "UNDEFINED"

# Ensure that pseudo builds both 32 bit and 64 bit binaries, as we need the
# 32 bit pseudo to execute the 32 bit external toolchain binaries without
# errors or warnings.
NO32LIBS ?= "0"

# Ensure that we only attempt to package up locales which are available in the
# external toolchain. In the future, we should examine the external toolchain
# sysroot and determine this accurately.
GLIBC_GENERATE_LOCALES_remove = "en_US.UTF-8"

# Don't ship any toolchain binaries in the sdk for the time being
# FIXME: find a way to do this just for the recipes which include those
# binaries. Potentially we could replace the packagegroup with an alternative
# version which includes nothing, or provide a bbclass which filters it out at
# RecipeParsed time.
#TOOLCHAIN_HOST_TASK ?= "nativesdk-packagegroup-sdk-host meta-environment-${TRANSLATED_TARGET_ARCH}"

# Add the external toolchain to the sdk setup script PATH
INHERIT += "toolchain-scripts-external"

localedir = "${exec_prefix}/lib/locale"

CSL_TARGET_SYS_powerpc ?= "powerpc-linux-gnu powerpc-mentor-linux-gnu"
CSL_TARGET_SYS_powerpc64 ?= "powerpc-linux-gnu powerpc-mentor-linux-gnu"
CSL_TARGET_SYS_arm ?= "arm-none-linux-gnueabi arm-mentor-linux-gnueabi"
CSL_TARGET_SYS_mips ?= "mips-linux-gnu mips-mentor-linux-gnu"
CSL_TARGET_SYS_mipsel ?= "mips-linux-gnu mips-mentor-linux-gnu"
CSL_TARGET_SYS_mips64 ?= "mips64-nlm-linux-gnu mips-linux-gnu mips-mentor-linux-gnu"
CSL_TARGET_SYS_x86-64 ?= "i686-pc-linux-gnu i686-mentor-linux-gnu x86_64-linux-gnu x86_64-amd-linux-gnu"
CSL_TARGET_SYS_i686 ?= "i686-pc-linux-gnu i686-mentor-linux-gnu"
CSL_TARGET_SYS_i586 ?= "i686-pc-linux-gnu i686-mentor-linux-gnu"
CSL_TARGET_SYS = "${TARGET_SYS}"

def csl_target_sys(d):
    toolchain_path = d.getVar('EXTERNAL_TOOLCHAIN', True)

    for triplet in d.getVar('CSL_TARGET_SYS', True).split():
        gcc = os.path.join(toolchain_path, 'bin', triplet + '-gcc')
        if os.path.exists(gcc):
            return triplet + '-'
    return '${TARGET_SYS}-'

TARGET_PREFIX = "${@csl_target_sys(d)}"

PREFERRED_PROVIDER_linux-libc-headers = "external-sourcery-toolchain"
PREFERRED_PROVIDER_linux-libc-headers-dev = "external-sourcery-toolchain"
PREFERRED_PROVIDER_virtual/${TARGET_PREFIX}gcc = "external-sourcery-toolchain"
PREFERRED_PROVIDER_virtual/${TARGET_PREFIX}gcc-initial = "external-sourcery-toolchain"
PREFERRED_PROVIDER_virtual/${TARGET_PREFIX}gcc-intermediate = "external-sourcery-toolchain"
PREFERRED_PROVIDER_virtual/${TARGET_PREFIX}g++ = "external-sourcery-toolchain"
PREFERRED_PROVIDER_virtual/${TARGET_PREFIX}binutils = "external-sourcery-toolchain"
PREFERRED_PROVIDER_virtual/${TARGET_PREFIX}libc-for-gcc ?= "external-sourcery-toolchain"
PREFERRED_PROVIDER_virtual/${TARGET_PREFIX}libc-initial ?= "external-sourcery-toolchain"
PREFERRED_PROVIDER_virtual/${TARGET_PREFIX}compilerlibs = "external-sourcery-toolchain"
PREFERRED_PROVIDER_libgcc = "external-sourcery-toolchain"
PREFERRED_PROVIDER_eglibc ?= "external-sourcery-toolchain"
PREFERRED_PROVIDER_virtual/libc ?= "external-sourcery-toolchain"
PREFERRED_PROVIDER_virtual/libintl ?= "external-sourcery-toolchain"
PREFERRED_PROVIDER_virtual/libiconv ?= "external-sourcery-toolchain"
PREFERRED_PROVIDER_gdbserver = "external-sourcery-toolchain"

# No need to re-compile the locale files
GLIBC_INTERNAL_USE_BINARY_LOCALE = "precompiled"
ENABLE_BINARY_LOCALE_GENERATION = ""

TOOLCHAIN_OPTIONS = " --sysroot=${STAGING_DIR_HOST}"

def sourcery_get_sysroot(d):
    import subprocess
    sysroot_cmd = "${TARGET_PREFIX}gcc ${TARGET_CC_ARCH} -print-sysroot"
    try:
        toolchain_sysroot = bb.process.run(bb.data.expand(sysroot_cmd, d),
                                           stderr=subprocess.PIPE,
                                           env={"PATH": d.getVar('PATH', True)})[0].rstrip()
    except bb.process.CmdError as exc:
        bb.fatal(str(exc))
    else:
        return toolchain_sysroot

EXTERNAL_TOOLCHAIN_SYSROOT = "${@sourcery_get_sysroot(d)}"

CSL_IS_PRO = "${@'1' if os.path.exists('${EXTERNAL_TOOLCHAIN}/license') else '0'}"

LDEMULATION = ""
LDEMULATION_ENDIAN = "${@'bt' if 'bigendian' in TUNE_FEATURES.split() else 'lt'}"
LDEMULATION_BITS = "${@'64' if 'n64' in TUNE_FEATURES.split() else '32'}"
LDEMULATION_mips64 = "elf${LDEMULATION_BITS}${LDEMULATION_ENDIAN}smip${@bb.utils.contains('TUNE_FEATURES', 'n32', 'n32', '', d)}"
TUNE_LDARGS += "${@'-m ${LDEMULATION}' if LDEMULATION else ''}"

# Ensure that the licensing variables are available to the toolchain.
export MGLS_LICENSE_FILE
export LM_LICENSE_FILE

# Unfortunately, the CSL ia32 toolchain has non-prefixed binaries in its
# bindir (e.g. gcc, ld). To avoid this messing up our build, we avoid adding
# this bindir to our PATH, and instead add symlinks to the prefixed binaries
# to our staging toolchain bindir.

ERROR_QA[type] ?= "list"
python toolchain_metadata_setup () {
    import subprocess
    d = e.data

    # Ensure that changes to toolchain licensing don't affect checksums
    d.appendVar('BB_HASHBASE_WHITELIST', ' MGLS_LICENSE_FILE LM_LICENSE_FILE')

    l = d.createCopy()
    l.finalize()
    oe_import(l)

    # Remove already-added toolchain install paths from the PATH, as they can
    # break the build (in particular, the ia32 toolchain, as it provdes
    # non-prefixed binaries).
    install_prefix_default = bb.data.expand('${HOME}/CodeSourcery/', l)
    path = d.getVar('PATH', False).split(':')
    path = filter(lambda p: not p.startswith(install_prefix_default), path)
    d.setVar('PATH', ':'.join(path))

    external_toolchain = l.getVar('EXTERNAL_TOOLCHAIN', True)
    if not external_toolchain or external_toolchain == 'UNDEFINED':
        bb.fatal("Error: EXTERNAL_TOOLCHAIN must be set to the path to your sourcery toolchain")

    if not os.path.exists(external_toolchain):
        bb.fatal("Error: EXTERNAL_TOOLCHAIN path '%s' does not exist" % external_toolchain)

    populate_toolchain_links(l)

    # The external toolchain may not have been built with the yocto preferred
    # gnu hash setting, so ensure that the corresponding sanity check is a
    # warning, not an error.
    error_qa = oe.data.typed_value('ERROR_QA', l)
    if 'ldflags' in error_qa:
        error_qa.remove('ldflags')
        d.setVar('ERROR_QA', ' '.join(error_qa))
        d.appendVar('WARN_QA', ' ldflags')

    #
    # Determine if '-msgxx-glibc' is a valid toolchain option.
    # If so then we need to use it to ensure that the libraries included with
    # the toolchain are used rather than the build host native libraries.
    #
    try:
        sysroot_cmd_output = bb.process.run(bb.data.expand("${TARGET_PREFIX}gcc -msgxx-glibc -print-sysroot", l),
                                            env={"PATH": l.getVar('PATH', True)})[0].rstrip()
    except bb.process.CmdError as exc:
        bb.fatal(str(exc))
    else:
        if "unrecognized command line option '-msgxx-glibc'" not in sysroot_cmd_output:
            d.appendVar('TUNE_CCARGS', ' -msgxx-glibc')
}
toolchain_metadata_setup[eventmask] = "bb.event.ConfigParsed"
addhandler toolchain_metadata_setup

def populate_toolchain_links(d):
    import errno
    import os
    from glob import glob

    pattern = d.expand('${EXTERNAL_TOOLCHAIN}/bin/${TARGET_PREFIX}*')
    files = glob(pattern)
    if not files:
        bb.fatal("Unable to populate toolchain binary symlinks in %s" % pattern)


    origenv = d.getVar('BB_ORIGENV', False)
    logname = origenv.get('LOGNAME', 'unknown')
    tmpdir = os.path.join(origenv.get('TMPDIR', '/tmp'), 'sourcery-%s' % logname)
    bb.utils.mkdirhier(tmpdir)

    bindir = d.getVar('STAGING_BINDIR_TOOLCHAIN', True)
    bb.utils.mkdirhier(bindir)
    for f in files:
        base = os.path.basename(f)
        newpath = os.path.join(bindir, base)
        if not os.path.exists(newpath):
            with open(newpath, 'w') as new:
                new.write('#!/bin/sh\n')
                new.write('export TMPDIR="{0}"\n'.format(tmpdir))
                new.write('exec "{0}" "$@"\n'.format(f))
                os.fchmod(new.fileno(), 0755)

    # Ensure that we have a ld.bfd available, now that KERNEL_LD uses it
    ld = d.expand('${TARGET_PREFIX}ld')
    ld_bfd = os.path.join(bindir, ld + '.bfd')
    if not os.path.exists(ld_bfd):
        try:
            os.symlink(ld, ld_bfd)
        except OSError as exc:
            if exc.errno != errno.EEXIST:
                bb.fatal("Unable to populate toolchain binary symlink for %s: %s" % (ld_bfd, exc))

require conf/distro/include/csl-versions.inc

def get_gcc_version_prefix(d):
    ver = d.getVar('CSL_VER_GCC', True)
    if ver:
        components = ver.split('.')
        if len(components) > 1:
            return '.'.join(components[:2])
    else:
        return '4.7%'

# Prefer a matching gcc version
GCCVERSION ?= "${@get_gcc_version_prefix(d)}%"

BUILDCFG_VARS += "CSL_VER_MAIN EXTERNAL_TOOLCHAIN"
SDKGCCVERSION ?= "${GCCVERSION}"
PREFERRED_VERSION_gcc-crosssdk ?= "${SDKGCCVERSION}"
PREFERRED_VERSION_gcc-crosssdk-initial ?= "${SDKGCCVERSION}"
PREFERRED_VERSION_gcc-crosssdk-intermediate ?= "${SDKGCCVERSION}"
PREFERRED_VERSION_nativesdk-gcc-runtime ?= "${SDKGCCVERSION}"
PREFERRED_VERSION_nativesdk-libgcc ?= "${SDKGCCVERSION}"
