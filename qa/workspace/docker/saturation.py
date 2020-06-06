from test_pylibs.test_utils import init_logs, get_orders_amount, check_saturation, enable_electrums,\
                                   check_proxy_connection, start_mm2_node, init_connection
import time
import pytest


def mainloop(maker: object, taker: object, coin_a: str, coin_b: str, log: object):
    time_sleep = 45
    step = 20
    orders_broadcast = 15
    info_orders = orders_broadcast
    check = True  # init "pass" value
    log.info("Entering main test loop")
    while check:
        log.debug("Clearing up previous orders in %s s", str(time_sleep))
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
        log.info("Maker node orders available: %s", str(maker_orders))
        taker_orders = get_orders_amount(taker, coin_a, coin_b).get('amount')
        log.info("Taker node orders available: %s", str(taker_orders))
        check = check_saturation(maker_orders, taker_orders)
        check_str = 'passed' if check else 'failed'  # bool can not be explicitly converted to str
        log.info("Maker to Taker orders amount check: %s", str(check_str))
        check = check_saturation(orders_broadcast, taker_orders)
        check_str = 'passed' if check else 'failed'
        log.info("Taker to Created orders amount check: %s", str(check_str))
        log.debug("Test iteration finished")
        info_orders = orders_broadcast
        orders_broadcast += step
    log.info("Test result. Network saturated with orders broadcasted: %s", str(info_orders))


def test_saturation():
    log = init_logs()
    coin_a = 'WSG'
    coin_b = 'BSG'
    mm_nodes = ["172.23.0.20", "172.23.0.22", "172.23.0.18"]
    log.info("Connecting to mm2 nodes")
    proxies = init_connection("RPC_PASSWORD", mm_nodes)
    electrums_base = ["node.sirseven.me:15001", "node.sirseven.me:25001"]
    electrums_rel = ["node.sirseven.me:15005", "node.sirseven.me:25005"]
    log.info("mm2 nodes connected, coins enabled")
    for node in mm_nodes:
        enable_electrums(proxies[node], electrums_base, electrums_rel, coin_a, coin_b)
    mainloop(proxies['172.23.0.20'], proxies['172.23.0.22'], coin_a, coin_b, log)
