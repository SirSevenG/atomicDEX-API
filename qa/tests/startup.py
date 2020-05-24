from test_pylibs.test_utils import init_logs, start_mm2_node
import os


def main():
    log = init_logs()
    mode = os.environ.get('MODE')
    start_mm2_node(log, mode)


if __name__ == '__main__':
    main()
