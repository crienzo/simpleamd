Summary: A library for answering machine detection
Name: simpleamd
Version: 1.1.0
Release: 1%{?dist}
License: MIT
Group: System Environment/Libraries
URL: https://github.com/crienzo/simpleamd
Source: simpleamd-%{version}.tar.gz
BuildRequires: autoconf automake libtool

%description
The simpleamd package contains a library for performing answering
machine detection.

%package devel
Summary: Development tools for programs to detect answering machines from audio.
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
The simpleamd-devel package contains header files and documentation necessary
for developing programs using the simpleamd library.

%prep
%setup -c

%build
./autogen.sh
%configure
make

%install
make DESTDIR=$RPM_BUILD_ROOT install
rm -f $RPM_BUILD_ROOT%{_libdir}/*.la

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%{_libdir}/libsimpleamd.so.*

%files devel
%{_bindir}/*
%{_includedir}/*
%{_libdir}/libsimpleamd*.so

%changelog
* Thu Apr 27 2017 James Le Cuirot <james.le-cuirot@yakara.com> 1.1.0-1
- Start vad in initial state so that SILENCE event can be emitted first
* Mon Jun 8 2015 Chris Rienzo <chris@rienzo.com> 1.0.0-1
- Initial revision
