from test_pylibs.test_utils import init_logs, get_orders_amount, check_saturation, enable_electrums,\
                                   check_proxy_connection, start_mm2_node, init_connection
import time
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
    log = init_logs()
    coin_a = 'WSG'
    coin_b = 'BSG'
    mode = os.environ.get('MODE')
    start_mm2_node(log, mode)
    proxies = init_connection("RPC_PASSWORD", ["mm_a", "mm_b", "mm_seed"])
    mainloop(proxies['mm_a'], proxies['mm_b'], coin_a, coin_b, log)