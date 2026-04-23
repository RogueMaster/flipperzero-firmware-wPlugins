Name:           infrafid
Version:        %{_version}
Release:        1%{?dist}
Summary:        InfraFi IR WiFi credential receiver daemon

License:        MIT
URL:            https://github.com/amd989/infrafi
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  kernel-headers
BuildRequires:  systemd-rpm-macros

%global debug_package %{nil}

%description
Receives WiFi credentials transmitted via infrared (RC-6 or NEC protocols) from a
Flipper Zero running the InfraFi app. Automatically connects to the
transmitted WiFi network using NetworkManager, systemd-networkd, or ifupdown.

Designed for headless Linux servers with CIR (Consumer IR) receivers such as
the ITE8708 found in Intel NUCs.

%prep
%setup -q

%build
cd daemon
make %{?_smp_mflags}

%install
install -D -m 0755 daemon/infrafid %{buildroot}%{_bindir}/infrafid
install -D -m 0644 daemon/infrafid.service %{buildroot}%{_unitdir}/infrafid.service
install -D -m 0644 daemon/99-infrafid-ir.rules %{buildroot}%{_udevrulesdir}/99-infrafid-ir.rules

%files
%license LICENSE.md
%doc README.md
%{_bindir}/infrafid
%{_unitdir}/infrafid.service
%{_udevrulesdir}/99-infrafid-ir.rules

%post
%systemd_post infrafid.service

%preun
%systemd_preun infrafid.service

%postun
%systemd_postun_with_restart infrafid.service

%changelog
