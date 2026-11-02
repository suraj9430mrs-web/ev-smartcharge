# EV SmartCharge — GitHub Repository Setup

## 1. Create a GitHub Repository

1. Go to https://github.com/new
2. Repository name: `ev-smartcharge`
3. Description: `Efficient EV Charging Slot Assignment — C++ Backend with SQLite Database`
4. Visibility: **Public** or **Private**
5. Do NOT initialize with README (we already have one)
6. Click **Create repository**

## 2. Initialize and Push

```bash
cd "/Users/surajkumar/Downloads/EV CHarging"

# Initialize git
git init

# Add all files
git add .

# Create the first commit
git commit -m "Initial commit: EV SmartCharge platform with C++ backend, SQLite database, and responsive frontend"

# Set the main branch
git branch -M main

# Add your remote (replace with your repo URL)
git remote add origin https://github.com/<your-username>/ev-smartcharge.git

# Push
git push -u origin main
```

## 3. Repository Structure

```
ev-smartcharge/
├── backend/
│   ├── api/            # REST API routes
│   ├── db/             # SQLite database layer
│   ├── lib/            # cpp-httplib header
│   ├── models/         # Vehicle & Station models
│   ├── services/       # C++ charging engine
│   ├── structures/     # DSA implementations (Stack, Queue, LinkedList, Array)
│   ├── utils/          # Crypto utilities (SHA-256, PBKDF2)
│   ├── main.cpp        # Server entry point
│   └── build.sh        # Build script
├── frontend/
│   ├── css/            # Stylesheets
│   ├── js/             # JavaScript (API client, main, admin)
│   ├── pages/          # HTML pages
│   ├── index.html      # Homepage
│   ├── admin.html      # Admin console
│   ├── robots.txt      # SEO
│   └── sitemap.xml     # SEO
├── docs/               # Documentation
├── tests/              # Test suite
├── original_backup/    # Original team source code
├── Dockerfile          # Container build
├── CMakeLists.txt      # CMake build
└── README.md           # Project documentation
```

## 4. Protect Main Branch (Optional)

1. Go to Settings → Branches → Add rule
2. Branch name pattern: `main`
3. Enable: Require pull request reviews before merging
4. Click **Create**

