Name: nosefart
Summary: Nosefart player for Nintendo NES audio files
Version: 2.3
Release: 1
License: GPL
Group: Applications/Multimedia
URL: http://nosefart.sourceforge.net/
Source0: http://prdownloads.sourceforge.net/nosefart/%name-%version-mls.tar.bz2
BuildRoot: %{_tmppath}/%{name}-buildroot
BuildRequires: xmms-devel

%description
Nosefart plays .nsf audio files that were ripped from games for the Nintendo
Entertainment System.

%package xmms_plugin
Requires: xmms
Summary: Nosefart plugin for XMMS
Group: Applications/Multimedia

%description xmms_plugin
NSF audio plugin for XMMS.

%prep
%setup -q -n %name-%version-mls

%build
make clean
make

%install
rm -rf $RPM_BUILD_ROOT 
make PREFIX=$RPM_BUILD_ROOT%{_prefix} install
cd src/xmms
./configure
make 
install -m 755 -D .libs/libnosefart.so %buildroot%_libdir/xmms/Input/libnosefart.so

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
%doc README* CHANGES
%{_bindir}/nosefart

%files xmms_plugin
%_libdir/xmms/Input/libnosefart.so

%changelog
* Fri Apr 16 2004 Matthew Strait <quadong@users.sf.net> 2.2-1
- Updated for 2.2

* Fri Apr 16 2004 Matthew Strait <quadong@users.sf.net> 2.1-1
- No change.  (gnosefart update.)

* Fri Apr 16 2004 Matthew Strait <quadong@users.sf.net> 2.0-1
- Added gnosefart.

* Sun Apr 4 2004 Matthew Strait <quadong@users.sf.net> 1.92k-1
- Updated for k and made it actually build the xmms plugin.

* Sun Mar 21 2004 Matthew Strait <quadong@users.sf.net> 1.92j-1
- Updated for j.

* Wed Feb 25 2004 Julian Dunn <jdunn@opentrend.net> 1.92i
- repackaged for RedHat 9

* Tue Oct 28 2003 Götz Waschk <goetz@plf.zarb.org> 1.92i-1plf
- initial package
