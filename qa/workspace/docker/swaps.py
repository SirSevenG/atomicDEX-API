from test_pylibs.test_utils import init_logs, enable_electrums, swap_status_iterator, init_connection, rand_value,\
                                   swaps_all, swaps_success
import time
from decimal import Decimal
import pytest


def mainloop(maker: object, taker: object, coin_a: str, coin_b: str, log: object):
    time_sleep = 30
    swap_uuids = []
    swaps_to_run = 10
    log.info("Entering main test loop")
    log.debug("Clearing up previous orders in %s s", str(time_sleep))
    maker.cancel_all_orders(cancel_by={'type': 'All'})  # reset orders
    taker.cancel_all_orders(cancel_by={'type': 'All'})
    time.sleep(time_sleep)
    price1 = rand_value(0.81, 0.995)  # gen prices and volumes for swap
    volume1 = rand_value(0.5, 0.9)
    volume_to_swap = "{0:.8f}".format((Decimal(volume1)*Decimal(0.9))/swaps_to_run)
    log.info("Creating maker order in %s s", str(time_sleep))
    res = maker.setprice(base=coin_a, rel=coin_b, price=price1, volume=volume1, cancel_previous=False)
    log.debug("Response: %s", str(res))
    for i in range(swaps_to_run):
        resp = taker.buy(base=coin_a, rel=coin_b, price=price1, volume=volume_to_swap)
        log.debug("Create order, number: %s\n%s", str(i + 1), str(resp))
        if resp.get("result"):
            swap_uuids.append((resp.get("result")).get("uuid"))
        else:
            swap_uuids.append((resp.get("error")))
        time.sleep(10)
    log.debug("uuids: %s", str(swap_uuids))
    time.sleep(10)
    log.info("Waiting for swaps to finish")
    result = swap_status_iterator(swap_uuids, taker)
    log.info("Test result: %s", str(result))
    log.info("Out of %s swaps %s finished successfully", swaps_all(result), swaps_success(result))


def test_swaps():
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
