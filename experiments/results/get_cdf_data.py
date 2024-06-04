
import numpy as np
import sys
import os
import pickle
import time

system = sys.argv[1]
test = sys.argv[2]
path = sys.argv[3]

# Configurations.
DATA_DIR = "{}/{}/".format(path, test)

MAX_X = 100000
TXN_MAX_X = 10000

def load_from_cache(cache_path, origin_file_path):
  if not os.path.exists(cache_path):
    return False, {}
  if not os.path.exists(origin_file_path):
    print("Error: origin_file_path passed to load_from_cache does not exist")
    exit()
  inf = open(cache_path, 'rb')
  my_dict = pickle.load(inf)
  m_time = os.path.getmtime(origin_file_path)
  if m_time > my_dict['time']:
    return False, {}
  return True, my_dict['object']

def cache_to_file(cache_path, my_object):
  my_dict = {'object': my_object, 'time': time.time()}
  outf = open(cache_path, 'wb')
  pickle.dump(my_dict, outf)

def get_cdf(data, limit_x=False):
  cdf_x = []
  cdf_y = []
  counter = 0
  for i in data:
    if counter % 50 == 0:
      if limit_x == True and i >= TXN_MAX_X:
        cdf_x.append(TXN_MAX_X)
      else:
        cdf_x.append(i)
    counter += 1
  cdf_x.sort()
  index = 0
  size = len(cdf_x)
  for d in cdf_x:
    y = index / size * 100
    cdf_y.append(round(y, 3))
    index += 1
  for i in range(5):
    cdf_x.append(MAX_X)
    cdf_y.append(100.000)
  return cdf_x, cdf_y

def get_cdf_no_sample(data, limit_x=False):
  cdf_x = []
  cdf_y = []
  counter = 0
  for i in data:
    if limit_x == True and i >= TXN_MAX_X:
      cdf_x.append(TXN_MAX_X)
    else:
      cdf_x.append(i)
    counter += 1
  cdf_x.sort()
  index = 0
  size = len(cdf_x)
  for d in data:
    y = index / size * 100
    cdf_y.append(round(y, 3))
    index += 1
  for i in range(5):
    cdf_x.append(MAX_X)
    cdf_y.append(100.000)
  return cdf_x, cdf_y

def get_micro_cdf():
  # systems = all_systems
  print("Processing {}".format(system))
  outf = DATA_DIR + "lat-{}-grant-cdf".format(system)
  outf_output = open(outf, 'w')
  latency_data = []
  inf  = DATA_DIR + "lat-{}-grant".format(system)
  data = np.fromfile(inf, sep = '\n')
  latency_data = np.concatenate([latency_data, data])
  cdf_x, cdf_y = get_cdf(latency_data)
  for i in range(len(cdf_x)):
    x = cdf_x[i]
    y = cdf_y[i]
    outf_output.write("{},{}\n".format(x, y))
  outf_output.close()

def get_txn_cdf():
  txn_id_map = {
    "tpcc": {
      "1": "NEW", #new_order
      "2": "PAY", #payment
      "3": "OS", #order_status
      "4": "DLY", #delivery
      "5": "SL", #stock_level
    },
    "tatp": {
      "1": "GS", #get_subscriber
      "2": "GD", #get_new_destination
      "3": "GA", #get_access
      "4": "US", #update_subscriber
      "5": "UL", #update_location
      "6": "IF", #insert_call_forwarding
      "7": "DF", #delete_call_forwarding
    }
  }
  current_txn_map = {}
  if "tpcc" in test:
    current_txn_map = txn_id_map["tpcc"]
  elif "tatp" in test:
    current_txn_map = txn_id_map["tatp"]

  latency_data = {}
  cache_path = DATA_DIR + "cache-{}-{}-txn".format(system, test)
  inf = DATA_DIR + "lat-{}-raw".format(system)
  latency_data[system] = {}
  flag, txn_latency_data = load_from_cache(cache_path, inf)
  if flag == True:
    latency_data[system] = txn_latency_data
    print("Loaded {} from cache".format(system))
  else:
    print("Processing {}".format(system))
    inf_input = open(inf, 'r')
    lines = inf_input.readlines()
    for line in lines:
      current_line = line.rstrip('\n')
      split_list = current_line.split(',')
      log_type = split_list[3]
      if log_type != "t":
        continue
      latency = float(split_list[2]) / 1000.0
      txn_id = split_list[1]
      if txn_id not in latency_data[system]:
        latency_data[system][txn_id] = []
      latency_data[system][txn_id].append(latency)

  cache_to_file(cache_path, latency_data[system])
  print("Processing done")

  all_latency = {}
  all_latency[system] = []
  for data in latency_data[system].values():
    all_latency[system].extend(data)
  
  for txn_id in current_txn_map.keys():
    txn_type = current_txn_map[txn_id]
    print("Write {} {}".format(txn_type, system))
    outf = DATA_DIR + "lat-{}-{}".format(system, txn_type)
    outf_output = open(outf, 'w')
    cdf_x = []
    cdf_y = []
    if txn_type == "DLY" or txn_type == "OS" or txn_type == "SL":
      cdf_x, cdf_y = get_cdf_no_sample(
        np.array(latency_data[system][txn_id]), True)
    else:
      cdf_x, cdf_y = get_cdf(np.array(latency_data[system][txn_id]), True)
    for i in range(len(cdf_x)):
      x = cdf_x[i]
      y = cdf_y[i]
      outf_output.write("{},{}\n".format(x, y))
    outf_output.close()

  print("Write ALL {}".format(system))
  outf = DATA_DIR + "lat-{}-all".format(system)
  outf_output = open(outf, 'w')
  cdf_x, cdf_y = get_cdf(np.array(all_latency[system]), True)
  for i in range(len(cdf_x)):
    x = cdf_x[i]
    y = cdf_y[i]
    outf_output.write("{},{}\n".format(x, y))
  outf_output.close()

def get_lock_scale_cdf():
  testnames = ['lkscale-{}m'.format(i) for i in [1, 2, 5, 10]]
  for testname in testnames:
    print("Processing system {} test {}".format(system, testname))
    outf = DATA_DIR + "lat-{}-grant-cdf".format(system)
    outf_output = open(outf, 'w')
    latency_data = []
    inf  = DATA_DIR + "lat-{}-grant".format(system)
    data = np.fromfile(inf, sep = '\n')
    latency_data = np.concatenate([latency_data, data])
    cdf_x, cdf_y = get_cdf(latency_data)
    for i in range(len(cdf_x)):
      x = cdf_x[i]
      y = cdf_y[i]
      outf_output.write("{},{}\n".format(x, y))
    outf_output.close()


if "micro" in test:
  get_micro_cdf()
elif "tatp" in test or "tpcc" in test:
  get_txn_cdf()
elif "lkscale" in test:
  get_lock_scale_cdf()