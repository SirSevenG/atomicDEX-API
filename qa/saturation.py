from test_pylibs.test_utils import init_logs, get_orders_amount, check_saturation, enable_electrums,\
                                   check_proxy_connection
from test_pylibs.mm2node import MMnode
import time
import ujson
import os
import pytest


def mainloop(maker: object, taker: object, coin_a: str, coin_b: str, log: object):
    time_sleep = 45
    step = 20
    orders_broadcast = 15
    info_orders = orders_broadcast
    check = True  # init "pass" value
    log.info("Entering main test loop")
    while check:
        log.info("Clearing up previous orders in %s s", str(time_sleep))
        maker.cancel_all_orders(cancel_by={'type': 'All'})  # reset orders
        time.sleep(time_sleep)
        log.info("New iteration, orders to broadcast: %s", str(orders_broadcast))
        for i in range(orders_broadcast):
            log.debug("Order placing num: %s", str(i + 1))
            res = maker.setprice(base=coin_a, rel=coin_b, price='0.1', volume='1', cancel_previous=False)
            log.debug("Response: %s", str(res))
            assert res.get('result').get('uuid')
            time.sleep(1)
        time.sleep(time_sleep)  # time to propagate orders
        maker_orders = get_orders_amount(maker, coin_a, coin_b).get('amount')
        log.debug("Maker node orders available: %s", str(maker_orders))
        taker_orders = get_orders_amount(taker, coin_a, coin_b).get('amount')
        log.debug("Taker node orders available: %s", str(taker_orders))
        check = check_saturation(maker_orders, taker_orders)
        check_str = 'passed' if check else 'failed'  # bool can not be explicitly converted to str
        log.debug("Maker to Taker orders amount check: %s", str(check_str))
        check = check_saturation(orders_broadcast, taker_orders)
        check_str = 'passed' if check else 'failed'
        log.debug("Taker to Created orders amount check: %s", str(check_str))
        log.info("Test iteration finished")
        info_orders = orders_broadcast
        orders_broadcast += step
    log.info("Test result. Network saturated with orders broadcasted: %s", str(info_orders))


def test_saturation():
    """proxy: MMProxy, electrums_base: list, electrums_rel: list, base: str, rel: str"""
    log = init_logs()
    mm_nodes = ['127.0.0.1:9901', '127.0.0.1:9902', '127.0.0.1:9903']
    electrums_a = ["node.sirseven.me:15001", "node.sirseven.me:25001"]
    electrums_b = ["node.sirseven.me:35001", "node.sirseven.me:45001"]
    coin_a = 'WSG'
    coin_b = 'BSG'
    try:
        bindir_t = os.environ['BINDIRT']
        bindir_m = os.environ['BINDIRM']
        bindir_s = os.environ['BINDIRS']
    except KeyError:
        default_target_path = os.path.abspath(os.path.join(os.curdir, os.pardir, 'target', 'debug'))
        assert default_target_path
        bindir_m = os.path.join(default_target_path, 'maker')
        bindir_t = os.path.join(default_target_path, 'taker')
        bindir_s = os.path.join(default_target_path, 'seed')
        try:
            os.mkdir(bindir_m)
            os.mkdir(bindir_s)
            os.mkdir(bindir_t)
        except FileExistsError:
            pass
        if os.name == 'posix':
            bin_mm = "/mm2"
            command = 'cp'
        else:
            bin_mm = "\\mm2.exe"
            command = 'copy'
        call = command + ' ' + default_target_path + bin_mm + ' ' + bindir_t + bin_mm
        os.system(call)
        call = command + ' ' + default_target_path + bin_mm + ' ' + bindir_m + bin_mm
        os.system(call)
        call = command + ' ' + default_target_path + bin_mm + ' ' + bindir_s + bin_mm
        os.system(call)
    with open('saturation.json') as j:
        test_params = ujson.load(j)
    taker = MMnode(test_params.get('taker').get('seed'), '9901', test_params.get('seednodes'), bindir_t, 0)
    maker = MMnode(test_params.get('maker').get('seed'), '9902', test_params.get('seednodes'), bindir_m, 0)
    seed = MMnode(test_params.get('seednode').get('seed'), '9903', test_params.get('seednodes'), bindir_s, 1)
    seed.start()
    seed_proxy = seed.rpc_conn()
    assert check_proxy_connection(seed_proxy)
    enable_electrums(seed_proxy, electrums_a, electrums_b, coin_a, coin_b)
    taker.start()
    maker.start()
    log.info("Connecting to mm2 nodes")
    taker_proxy = taker.rpc_conn()
    maker_proxy = maker.rpc_conn()
    assert check_proxy_connection(maker_proxy)
    enable_electrums(maker_proxy, electrums_a, electrums_b, coin_a, coin_b)
    assert check_proxy_connection(taker_proxy)
    enable_electrums(taker_proxy, electrums_a, electrums_b, coin_a, coin_b)
    log.info("mm2 nodes connected, coins enabled")
    mainloop(maker_proxy, taker_proxy, coin_a, coin_b, log)
