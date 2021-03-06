# Work around bug in some versions regarding unaligned access
TARGET_CC_ARCH_append_arm = " -mno-unaligned-access"
TARGET_CC_KERNEL_ARCH_append_arm = " -mno-unaligned-access"

def toolchain_sanity_flags(d):
    flags = d.getVar('DEBUG_FLAGS_pn-external-sourcery-toolchain', True)
    if flags is None:
        flags = d.getVar('DEBUG_FLAGS', True)
    return flags.split()

python toolchain_kernel_fixup () {
    d = e.data
    if isinstance(e, bb.event.RecipeParsed) and oe.utils.inherits(d, 'kernel'):
        import re
        if d.getVar('TCMODE', True) == 'default':
            return

        toolchain_path = d.getVar('EXTERNAL_TOOLCHAIN', True)
        if not toolchain_path:
            return

        # Work around kernel build error due to unused-but-set-variable + Werror
        version = d.getVar('CSL_VER_GCC', False)
        num_version = [int(element) for element in version.split('.')]
        match = re.match('^([0-9]+\.[0-9]+)', version)
        if match:
            version = match.group(1)
            if num_version >= [4, 6]:
                d.appendVar('TARGET_CC_ARCH', ' -Wno-error=unused-but-set-variable')
                d.appendVar('TARGET_CC_KERNEL_ARCH', ' -Wno-error=unused-but-set-variable')
            if num_version >= [4, 7]:
                d.appendVar('TARGET_CC_ARCH', ' -Wno-error=maybe-uninitialized')
                d.appendVar('TARGET_CC_KERNEL_ARCH', ' -Wno-error=maybe-uninitialized')
}
addhandler toolchain_kernel_fixup

python toolchain_sanity_check () {
    if not isinstance(e, bb.event.BuildStarted):
        return

    if e.data.getVar('TCMODE', True) == 'default':
        return

    # Make sure the toolchain actually exists
    toolchain_path = e.data.getVar('EXTERNAL_TOOLCHAIN', True)
    if not toolchain_path:
        return

    d = e.data.createCopy()
    d.finalize()

    toolchain_sysroot = d.getVar('EXTERNAL_TOOLCHAIN_SYSROOT', True)
    if not os.path.exists(toolchain_sysroot):
        bb.fatal('Toolchain sysroot `%s` does not exist, please install or fix your Sourcery toolchain at %s' % (toolchain_sysroot, toolchain_path))

    def version(str):
        if str is None:
            return []

        elements = str.split('-')
        if len(elements) > 1:
            elements.pop()
        return elements

    # Check for a specified required toolchain version for releases
    toolchain_required_version = version(d.getVar('CSL_VER_REQUIRED', True))
    if toolchain_required_version:
        csl_ver_main = version(d.getVar('CSL_VER_MAIN', True))
        if csl_ver_main:
            if csl_ver_main != toolchain_required_version:
                bb.fatal('Found toolchain version `%s`, expected `%s`. Please install the supported toolchain.' % ('-'.join(csl_ver_main), '-'.join(toolchain_required_version)))

    # Check that debug files are available, to be packaged into -dbg
    if '-g0' in toolchain_sanity_flags(d):
        return

    if not os.path.exists(os.path.join(toolchain_path, 'license')):
        # Assuming use of a CodeBench Lite toolchain
        return

    lib_debugdir = bb.data.expand(os.path.join(toolchain_sysroot, 'lib', '.debug'), d)
    try:
        if os.listdir(lib_debugdir):
            return
    except OSError:
        pass

    bb.error("%s does not exist, or is empty" % lib_debugdir)
    bb.error('You need to install the debug info add-on via Sourcery CodeBench.')
    bb.fatal('Please refer to the trouble-shooting section of the Mentor Embedded Linux Users Manual')
}
addhandler toolchain_sanity_check
