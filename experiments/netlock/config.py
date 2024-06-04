import sys

cluster_name = "pro"
host_user = "ck/workspace"
switch_user = "ck/workspace"
local_home_dir = "/home/" + host_user + "/"
remote_server_home_dir = "/home/" + host_user + "/"
remote_switch_home_dir = "/home/" + switch_user + "/"
remote_switch_sde_dir  = "/bf-sde-9.5.2/"

id_to_username_dict = {"pro0": "ck", "pro1": "ck", "pro2": "ck", "pro3": "ck", "192.168.12.146": "ck"}

id_to_passwd_dict = {"pro0": "KeCheng", "pro1": "KeCheng", "pro2": "KeCheng", "pro3": "KeCheng", "192.168.12.146": "KeCheng"}

id_to_hostname_dict = {"pro0": "pro0", "pro1": "pro1", "pro2": "pro2", "pro3": "pro3", "192.168.12.146": "192.168.12.146"}

client_id_t1 = []
client_id_t2 = []
server_id = []

def conf_3_clients_1_servers():
     global client_id_t1, client_id_t2, server_id
     # tenant 1's client id. Normally we only need this.
     client_id_t1 = ["pro1"]
     # tenant 2's client id. It's for the two tenant situation.
     client_id_t2 = []
     # Lock server's id
     server_id = ["pro3"]
     print("Clients:", client_id_t1)
     print("Servers:", server_id)

conf_3_clients_1_servers()
switch_id = "192.168.12.146"