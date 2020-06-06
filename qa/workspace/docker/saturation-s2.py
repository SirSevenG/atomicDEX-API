from test_pylibs.test_utils import init_logs, get_orders_amount, check_saturation, enable_electrums,\
                                   check_proxy_connection, start_mm2_node, init_connection, rand_item, rand_value
import time
import pytest


def mainloop(maker: object, taker: object, coin_a: str, coin_b: str, log: object):
    time_sleep = 30
    orders_broadcast = 10
    orders_current = 0
    check = True  # init "pass" value
    log.info("Entering main test loop")
    while check:
        log.debug("Clearing up previous orders in %s s", str(time_sleep))
        time.sleep(time_sleep)
        for i in range(orders_broadcast):
            orders_current += 1
            # gen new price and volume values for each swap
            price = rand_value(0.09, 0.1)
            volume = rand_value(0.5, 0.9)
            node = rand_item([maker, taker])  # select on random client node to broadcast order
            log.debug("Order placing num: %s", str(orders_current))
            res = node.setprice(base=coin_a, rel=coin_b, price=price, volume=volume, cancel_previous=False)
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
    log.info("Test result. Network saturated with orders broadcasted: %s", str(orders_current))


def test_saturation():
    log = init_logs()
    coin_a = 'WSG'
    coin_b = 'BSG'
    mm_nodes = ["mm_a", "mm_b", "mm_seed"]
    log.info("Connecting to mm2 nodes")
    proxies = init_connection("RPC_PASSWORD", mm_nodes)
    electrums_base = ["node.sirseven.me:15001", "node.sirseven.me:25001"]
    electrums_rel = ["node.sirseven.me:15005", "node.sirseven.me:25005"]
    log.info("mm2 nodes connected, coins enabled")
    for node in mm_nodes:
        enable_electrums(proxies[node], electrums_base, electrums_rel, coin_a, coin_b)
    mainloop(proxies['mm_b'], proxies['mm_a'], coin_a, coin_b, log)
