# !bin/bash

rm -f .ssh/id_rsa .ssh/id_rsa.pub
ssh-keygen -f ~/.ssh/id_rsa -N "" >/dev/null
touch ~/.ssh/authorized_keys; chmod 600 ~/.ssh/authorized_keys;
touch ~/.ssh/known_hosts;     chmod 600 ~/.ssh/known_hosts;
touch ~/.ssh/config;          chmod 600 ~/.ssh/config;
cat ~/setup/authorized_keys.template | tee -a ~/.ssh/authorized_keys >/dev/null
