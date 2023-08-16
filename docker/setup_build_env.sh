#! /bin/sh
#

apt-get update
apt-get install -y --no-install-recommends apt-utils
apt-get install -y --no-install-recommends curl apt-transport-https ca-certificates software-properties-common gnupg wget

if [ ! -f /.dockerenv ]; then # in dev env
	apt-get remove docker docker-engine docker.io containerd runc
	curl -fsSL https://download.docker.com/linux/ubuntu/gpg | apt-key add -
	add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable"
	curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | bash
	curl https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor > /tmp/microsoft.gpg
	install -o root -g root -m 644 /tmp/microsoft.gpg /etc/apt/trusted.gpg.d/
	sh -c 'echo "deb [arch=amd64] https://packages.microsoft.com/repos/vscode stable main" > /etc/apt/sources.list.d/vscode.list'
	apt-get update
	apt-get install -y git git-lfs code docker-ce docker-ce-cli containerd.io docker-compose
	git lfs install
	usermod -a -G docker $USER
	dpkg --add-architecture i386

	# gdb stl pretty-printing
	mv ${HOME}/.gdbinit ${HOME}/.gdbinit.old &> /dev/null || true
	cat << EOF > ${HOME}/.gdbinit
python
from os.path import expanduser
home = expanduser("~")
import sys
sys.path.insert(0, f'{home}/.gdb-printers/python')
from libstdcxx.v6.printers import register_libstdcxx_printers
register_libstdcxx_printers (None)
end
EOF
	chown $SUDO_UID ${HOME}/.gdbinit

fi

curl -sL https://apt.kitware.com/keys/kitware-archive-latest.asc | apt-key add -
apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main'

curl -sL https://deb.nodesource.com/setup_10.x | bash - 
apt-get update 	# node setup does an update but just in case

apt-get install -y --no-install-recommends screen tmux vim make python dpkg file \
libsqlite3-dev rpm cmake gcc g++ libz-dev libcurl4-openssl-dev libssl-dev libasio-dev \
nodejs uuid-dev libhpdf-dev grub-efi-amd64 grub-efi-amd64-signed shim shim-signed \
mokutil cppcheck valgrind libgtest-dev

if [ -f /.dockerenv ]; then # docker env
	apt-get upgrade -y
	rm -rf /var/lib/apt/lists/*

	#compile gtest
	cd /usr/src/gtest
	cmake .
	make
	cp *.a /usr/lib
	mkdir -p /usr/local/lib/gtest
	ln -s /usr/lib/libgtest.a /usr/local/lib/gtest/libgtest.a
	ln -s /usr/lib/libgtest_main.a /usr/local/lib/gtest/libgtest_main.a

fi

# boost 1.78.0
cd /opt
pwd
wget -O boost_1_75_0.tar.gz https://sourceforge.net/projects/boost/files/boost/1.75.0/boost_1_75_0.tar.gz/download
tar -xf boost_1_75_0.tar.gz
cd boost_1_75_0
./bootstrap.sh
./b2 install
cd ../
rm -rf boost_1_75_0.tar.gz boost_1_75_0