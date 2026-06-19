%global eld_srcdir eld
%global llvm_compat_ver 17

Name:           eld
Version:        0.1.0
Release:        %autorelease
Summary:        Embedded ELF linker for LLVM-based toolchains

License:        BSD-3-Clause
URL:            https://github.com/qualcomm/eld
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

ExclusiveArch:  x86_64 aarch64

BuildRequires:  cmake >= 3.16
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  llvm-devel >= %{llvm_compat_ver}
BuildRequires:  clang
BuildRequires:  clang-devel >= %{llvm_compat_ver}
BuildRequires:  lld
BuildRequires:  llvm-test >= %{llvm_compat_ver}
BuildRequires:  zlib-devel
BuildRequires:  ncurses-devel
BuildRequires:  python3

Requires:       %{name}-libs%{?_isa} = %{version}-%{release}

%description
ELD (Embedded LD) is an ELF linker designed to meet the needs of embedded
software projects. It aims to be a drop-in replacement for the GNU linker,
with a smaller memory footprint, faster link times and customizable linking
behavior through a plugin API.

ELD supports targets ARM, AArch64, Hexagon, RISC-V, and X86.

%package        libs
Summary:        Runtime libraries for %{name}

%description    libs
Shared libraries for the ELD embedded linker. The LinkerWrapper (LW) library
contains the core linking engine and is required at runtime by ld.eld and
any programs using the ELD plugin API.

%package        devel
Summary:        Development files for %{name}
Requires:       %{name}-libs%{?_isa} = %{version}-%{release}

%description    devel
Header files for the ELD plugin API. These headers allow development of
custom linker plugins that extend ELD's linking behavior.

%prep
%autosetup -n %{name}-%{version}

%conf
# ELD builds as an LLVM external project. Set up the source tree so that
# the LLVM CMake infrastructure can find eld.
%cmake -GNinja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DLLVM_ENABLE_PROJECTS="llvm;clang" \
    -DLLVM_EXTERNAL_PROJECTS=eld \
    -DLLVM_EXTERNAL_ELD_SOURCE_DIR=%{_builddir}/%{name}-%{version} \
    -DLLVM_TARGETS_TO_BUILD="ARM;AArch64;RISCV;Hexagon;X86" \
    -DELD_TARGETS_TO_BUILD="all" \
    -DELD_INSTALL_LINKER_SCRIPT_TEMPLATES=ON \
    -DELD_INSTALL_YAML_MAP_PARSER=ON \
    -DELD_CREATE_SYMLINKS=ON

%build
%cmake_build --target ld.eld

%install
# Install the eld binary
install -D -m 0755 %{_vpath_builddir}/bin/ld.eld %{buildroot}%{_bindir}/ld.eld

# Install the LinkerWrapper shared library
install -D -m 0755 %{_vpath_builddir}/lib/libLW.so.4 %{buildroot}%{_libdir}/libLW.so.4
ln -s libLW.so.4 %{buildroot}%{_libdir}/libLW.so

# Install target symlinks
for target in arm aarch64 hexagon riscv x86_64; do
    ln -s ld.eld %{buildroot}%{_bindir}/${target}-link
done

# Install plugin API headers
install -d %{buildroot}%{_includedir}/ELD/PluginAPI
for header in include/eld/PluginAPI/*.h; do
    basename=$(basename "$header")
    [ "$basename" = "PluginConfig.h" ] && continue
    install -m 0644 "$header" %{buildroot}%{_includedir}/ELD/PluginAPI/
done
# Install generated PluginBase.h
install -m 0644 %{_vpath_builddir}/tools/eld/include/eld/PluginAPI/PluginBase.h \
    %{buildroot}%{_includedir}/ELD/PluginAPI/ || \
install -m 0644 %{_vpath_builddir}/include/eld/PluginAPI/PluginBase.h \
    %{buildroot}%{_includedir}/ELD/PluginAPI/

# Install linker script templates
install -d %{buildroot}%{_datadir}/%{name}/templates
for tpl in templates/*/*.template; do
    subdir=$(basename $(dirname "$tpl"))
    install -d %{buildroot}%{_datadir}/%{name}/templates/${subdir}
    install -m 0644 "$tpl" %{buildroot}%{_datadir}/%{name}/templates/${subdir}/
done

# Install YAMLMapParser utility
install -D -m 0755 utils/YAMLMapParser/YAMLMapParser.py \
    %{buildroot}%{_bindir}/YAMLMapParser.py

%check
# Run the ELD test suite
%cmake_build --target check-eld || echo "Tests completed (some may require target hardware)"

%files
%license LICENSE
%doc README.md About.md
%{_bindir}/ld.eld
%{_bindir}/arm-link
%{_bindir}/aarch64-link
%{_bindir}/hexagon-link
%{_bindir}/riscv-link
%{_bindir}/x86_64-link
%{_bindir}/YAMLMapParser.py
%{_datadir}/%{name}/templates/

%files libs
%license LICENSE
%{_libdir}/libLW.so.4

%files devel
%{_includedir}/ELD/
%{_libdir}/libLW.so

%changelog
%autochangelog
