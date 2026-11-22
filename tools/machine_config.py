#!/usr/bin/env python3
"""Read and validate the machine registry."""
import argparse
import json
import re
import sys


class MachineConfigError(ValueError):
    pass


def load_machine_configs(path):
    try:
        with open(path, encoding='utf-8') as config_file:
            configs = json.load(config_file)
    except OSError as exc:
        raise MachineConfigError(str(exc)) from exc
    except json.JSONDecodeError as exc:
        raise MachineConfigError('invalid JSON: %s' % exc) from exc

    if not isinstance(configs, list):
        raise MachineConfigError('expected a JSON array')

    machines = {}
    for index, config in enumerate(configs):
        if not isinstance(config, dict):
            raise MachineConfigError('entry %d must be a JSON object' % index)
        machine = config.get('machine')
        if not isinstance(machine, str) or not re.fullmatch(
                r'[A-Za-z0-9_]+', machine):
            raise MachineConfigError(
                'entry %d "machine" must be a machine target name' % index)
        if machine in machines:
            raise MachineConfigError('duplicate machine target %r' % machine)

        friendly_name = config.get('friendly_name')
        if not isinstance(friendly_name, str) or not friendly_name.strip():
            raise MachineConfigError(
                '%s: "friendly_name" must be a non-empty string' % machine)
        if any(ord(char) < 32 or char in '"\\' for char in friendly_name):
            raise MachineConfigError(
                '%s: "friendly_name" contains a character unsafe for '
                'the compiler command line' % machine)

        pcb = config.get('pcb')
        if not isinstance(pcb, str) or not re.fullmatch(r'[A-Za-z0-9_]+', pcb):
            raise MachineConfigError(
                '%s: "pcb" must be a PCB source name without .c' % machine)
        machines[machine] = {
            'machine': machine,
            'friendly_name': friendly_name,
            'pcb': pcb,
        }
    return machines


def load_machine_config(path, machine):
    machines = load_machine_configs(path)
    if machine not in machines:
        raise MachineConfigError('unknown machine target %r' % machine)
    return machines[machine]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('field', choices=('machines', 'friendly_name', 'pcb'))
    parser.add_argument('path')
    parser.add_argument('machine', nargs='?')
    args = parser.parse_args()
    if args.field != 'machines' and args.machine is None:
        parser.error('the machine target name is required for this field')
    if args.field == 'machines' and args.machine is not None:
        parser.error('a machine target name is not allowed with "machines"')

    try:
        if args.field == 'machines':
            machines = load_machine_configs(args.path)
            print(' '.join(machines))
        else:
            config = load_machine_config(args.path, args.machine)
            print(config[args.field])
    except MachineConfigError as exc:
        print('%s: %s' % (args.path, exc), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
