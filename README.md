# Net Calculator for Flipper Zero

**Net Calculator** is an offline IPv4 VLSM subnet calculator for Flipper Zero.

The application calculates subnet allocations based on a parent IPv4 network and the required number of usable hosts.

This application is the Flipper Zero port of Net Calculator.

## Features

- IPv4 VLSM subnet calculation
- Decimal IPv4 address input
- Parent prefix selection from /8 to /30
- Up to 16 subnet requests
- Automatic sorting from the largest to the smallest subnet
- Request list with individual request removal
- Network address calculation
- First usable host calculation
- Last usable host calculation
- Broadcast address calculation
- Parent network overflow detection
- Fully offline operation

## Usage

The main menu contains the following options:

- **IP** — enter the parent IPv4 address
- **Prefix** — set the parent network prefix
- **Add host request** — add the required number of usable hosts
- **Requests** — view or remove previously added requests
- **Calculate** — calculate the VLSM subnet allocation
- **Reset** — restore the default values and remove all requests

## IPv4 address

The IPv4 address is entered as four separate decimal octets.

Each octet accepts values from 0 to 255.

Example: 192.168.1.0

The entered address is automatically normalized to the network address of the selected parent prefix.

For example, entering 192.168.1.120/24 uses 192.168.1.0/24 as the parent network.

## Parent prefix

The parent network prefix can be set from /8 to /30.

Example: /24

The selected prefix defines the address space available for all requested subnets.

## Adding subnet requests

Select **Add host request** and enter the required number of usable hosts.

Each value is stored as a separate subnet request.

A maximum of 16 requests can be added.

## Managing requests

Select **Requests** to open the list of previously added host requests.

Controls:

- use Up and Down to select a request
- press OK to remove the selected request
- press Back to return to the main menu

Requests are displayed in the order in which they were added.

During calculation, the application creates a copy of the request list and sorts it from the largest to the smallest subnet. The order shown in the Requests menu is not changed.

## Calculation results

For every allocated subnet, the application displays:

- requested usable host count
- subnet prefix
- network address
- first usable host
- last usable host
- broadcast address

Use Up and Down to scroll through the results.

Press Back to return to the main menu.

## VLSM allocation

Each subnet reserves one network address and one broadcast address.

The remaining addresses are treated as usable host addresses.

Requests are allocated from the largest to the smallest subnet to reduce address-space fragmentation.

Allocation continues until all requests have been processed or the parent network runs out of available address space.

If a requested subnet does not fit inside the parent network, the application displays an overflow message.

## Example

Parent network: 192.168.1.0/24

Requested usable hosts:

- 100
- 50
- 20

The requests are allocated in the following order:

- 100 hosts require a /25 subnet
- 50 hosts require a /26 subnet
- 20 hosts require a /27 subnet

The application then displays the network, first host, last host and broadcast address for every allocated subnet.

## Default values

Selecting **Reset** restores the following values:

- IP address: 192.168.1.0
- Parent prefix: /24
- Subnet requests: 0

## Project origin

The original console implementation is available in the [VLSM Calculator repository](https://github.com/WolfRorDev/VLSM_Calculator).

Microsoft Store version: [MS Store](https://www.microsoft.com/store/apps/9N63M39HGQCL)

## Author

Copyright © 2026 Dominik Krzywański

Published under the **WolfRor** name.

GitHub: [WolfRorDev](https://github.com/WolfRorDev)

Publisher website [wolfror.com](https://wolfror.com)

## License

This project is licensed under the GNU General Public License version 3 only.

SPDX-License-Identifier: GPL-3.0-only

See the [LICENSE](LICENSE) file for the complete license text.

## Disclaimer

Net Calculator is an independent third-party application for Flipper Zero.

Flipper Zero is a trademark of Flipper Devices Inc. This project is not affiliated with, sponsored by, endorsed by, or approved by Flipper Devices Inc.
