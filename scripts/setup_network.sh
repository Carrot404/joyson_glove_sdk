#!/bin/bash

# Joyson Glove SDK - Network Setup Script
# This script helps configure network settings for connecting to the glove hardware

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# Default values
GLOVE_IP="192.168.10.123"
LOCAL_IP="192.168.10.100"
SUBNET_MASK="24"
INTERFACE=""

main() {
    echo "=========================================="
    echo "  Joyson Glove SDK - Network Setup"
    echo "=========================================="
    echo ""

    # Check if running as root
    if [ "$EUID" -ne 0 ]; then
        print_error "This script must be run as root (use sudo)"
        exit 1
    fi

    # Step 1: Detect network interfaces
    print_step "Detecting network interfaces..."
    echo ""
    ip link show | grep -E "^[0-9]+:" | awk '{print $2}' | sed 's/://' | while read iface; do
        if [ "$iface" != "lo" ]; then
            state=$(ip link show "$iface" | grep -o "state [A-Z]*" | awk '{print $2}')
            echo "  - $iface (state: $state)"
        fi
    done
    echo ""

    # Step 2: Select interface
    read -p "Enter network interface name (e.g., eth0, enp0s31f6): " INTERFACE

    if [ -z "$INTERFACE" ]; then
        print_error "Interface name cannot be empty"
        exit 1
    fi

    # Check if interface exists
    if ! ip link show "$INTERFACE" &>/dev/null; then
        print_error "Interface $INTERFACE does not exist"
        exit 1
    fi

    print_info "Selected interface: $INTERFACE"

    # Step 3: Configure IP address
    print_step "Configuring IP address..."
    echo ""
    echo "  Glove IP: $GLOVE_IP"
    echo "  Local IP: $LOCAL_IP/$SUBNET_MASK"
    echo ""
    read -p "Use default settings? (y/n): " use_default

    if [ "$use_default" != "y" ] && [ "$use_default" != "Y" ]; then
        read -p "Enter local IP address: " LOCAL_IP
        read -p "Enter subnet mask (e.g., 24): " SUBNET_MASK
    fi

    # Bring interface up
    print_info "Bringing interface up..."
    ip link set "$INTERFACE" up

    # Remove existing IP addresses
    print_info "Removing existing IP addresses..."
    ip addr flush dev "$INTERFACE" || true

    # Add new IP address
    print_info "Adding IP address $LOCAL_IP/$SUBNET_MASK to $INTERFACE..."
    ip addr add "$LOCAL_IP/$SUBNET_MASK" dev "$INTERFACE"

    # Step 4: Test connectivity
    print_step "Testing connectivity..."
    echo ""
    print_info "Pinging glove at $GLOVE_IP..."

    if ping -c 3 -W 2 "$GLOVE_IP" &>/dev/null; then
        print_info "✓ Connection successful!"
    else
        print_warn "✗ Cannot reach glove at $GLOVE_IP"
        print_warn "Please check:"
        echo "  1. Glove hardware is powered on"
        echo "  2. Network cable is connected"
        echo "  3. Glove IP address is correct"
    fi

    # Step 5: Display configuration
    echo ""
    print_step "Network configuration summary:"
    echo ""
    echo "  Interface: $INTERFACE"
    echo "  Local IP: $LOCAL_IP/$SUBNET_MASK"
    echo "  Glove IP: $GLOVE_IP"
    echo "  Gateway: (none)"
    echo ""

    # Display current configuration
    print_info "Current interface configuration:"
    ip addr show "$INTERFACE"
    echo ""

    # Step 6: Firewall configuration
    print_step "Checking firewall..."
    if command -v ufw &>/dev/null; then
        UFW_STATUS=$(ufw status | grep -o "Status: [a-z]*" | awk '{print $2}')
        if [ "$UFW_STATUS" = "active" ]; then
            print_warn "UFW firewall is active"
            read -p "Allow UDP port 8080? (y/n): " allow_port
            if [ "$allow_port" = "y" ] || [ "$allow_port" = "Y" ]; then
                ufw allow 8080/udp
                print_info "UDP port 8080 allowed"
            fi
        else
            print_info "UFW firewall is inactive"
        fi
    else
        print_info "UFW not installed, skipping firewall configuration"
    fi

    # Step 7: Save configuration (optional)
    echo ""
    read -p "Save configuration to /etc/network/interfaces? (y/n): " save_config

    if [ "$save_config" = "y" ] || [ "$save_config" = "Y" ]; then
        BACKUP_FILE="/etc/network/interfaces.backup.$(date +%Y%m%d_%H%M%S)"
        print_info "Backing up current configuration to $BACKUP_FILE..."
        cp /etc/network/interfaces "$BACKUP_FILE"

        print_info "Adding configuration to /etc/network/interfaces..."
        cat >> /etc/network/interfaces <<EOF

# Joyson Glove configuration (added $(date))
auto $INTERFACE
iface $INTERFACE inet static
    address $LOCAL_IP
    netmask $(cidr_to_netmask $SUBNET_MASK)
EOF
        print_info "Configuration saved"
    fi

    # Step 8: Test SDK connection
    echo ""
    print_step "Testing SDK connection..."
    read -p "Run basic connectivity test? (y/n): " run_test

    if [ "$run_test" = "y" ] || [ "$run_test" = "Y" ]; then
        # Create simple test program
        TEST_SCRIPT="/tmp/glove_test.sh"
        cat > "$TEST_SCRIPT" <<'EOF'
#!/bin/bash
echo "Sending test UDP packet to glove..."
# Send a simple read status command (motor 1)
echo -ne '\xEE\x0F\x00\x01\x01\x00\x00\x00\x7E' | timeout 2 nc -u -w 1 192.168.10.123 8080
if [ $? -eq 0 ]; then
    echo "✓ UDP communication successful"
else
    echo "✗ No response from glove"
fi
EOF
        chmod +x "$TEST_SCRIPT"
        bash "$TEST_SCRIPT"
        rm "$TEST_SCRIPT"
    fi

    # Done
    echo ""
    echo "=========================================="
    print_info "Network setup completed!"
    echo "=========================================="
    echo ""
    echo "Next steps:"
    echo "  1. Test connection: ping $GLOVE_IP"
    echo "  2. Run SDK examples: cd build && ./basic_motor_control"
    echo "  3. Monitor traffic: sudo tcpdump -i $INTERFACE udp port 8080"
    echo ""
}

# Helper function: Convert CIDR to netmask
cidr_to_netmask() {
    local cidr=$1
    local mask=""
    local full_octets=$((cidr / 8))
    local partial_octet=$((cidr % 8))

    for ((i=0; i<4; i++)); do
        if [ $i -lt $full_octets ]; then
            mask="${mask}255"
        elif [ $i -eq $full_octets ]; then
            mask="${mask}$((256 - 2**(8-partial_octet)))"
        else
            mask="${mask}0"
        fi
        [ $i -lt 3 ] && mask="${mask}."
    done

    echo "$mask"
}

# Run main function
main "$@"
