Name: gnosefart
Summary: Graphical player for Nintendo NES audio files
Version: 1.1
Release: 1
License: GPL
Group: Applications/Multimedia
URL: http://nosefart.sourceforge.net/
Source0: http://prdownloads.sourceforge.net/nosefart/%name-%version.tar.bz2
BuildRoot: %{_tmppath}/nosefart-buildroot
BuildRequires: gtk2-devel
Requires: nosefart >= 2.2, gtk2

%description
gnosefart plays .nsf audio files that were ripped from games for the Nintendo
Entertainment System.  It's a GTK front end for nosefart.

%prep
%setup -q -n %name-%version

%build
./configure --prefix=$RPM_BUILD_ROOT%{_prefix}
make

%install
make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%{_bindir}/gnosefart
%attr(0644,root,root) %{_datadir}/applications/%{name}.desktop
%attr(0644,root,root) %{_datadir}/pixmaps/%{name}.png

%changelog
* Fri May 01 2004 Matthew Strait <quadong@users.sf.net> 1.1-1
- updated for 1.1
* Fri Apr 23 2004 Matthew Strait <quadong@users.sf.net> 1.0-1
- updated for 1.0
* Fri Apr 16 2004 Matthew Strait <quadong@users.sf.net> 0.9-1
- initial package
