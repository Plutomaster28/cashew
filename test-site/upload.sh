#!/bin/bash
# Upload test site to Cashew node
# This is a mock script - actual upload would use the Cashew API

echo "Mock upload script - demonstrates how it would work"
echo ""
echo "In a real implementation, this would:"
echo "1. Read index.html and style.css"
echo "2. Hash them with BLAKE3"
echo "3. POST to http://localhost:8080/api/thing/upload"
echo "4. Node stores them and returns Thing ID (hash)"
echo "5. You access via: http://localhost:8081/thing/<hash>"
echo ""
echo "For now, the Perl gateway serves from node's API directly."
