let socket;
let username;

window.onload = function () {

    username = localStorage.getItem("username");

    if (!username) {
        alert("Username not found. Redirecting to login.");
        window.location.href = "index.html";
        return;
    }

    socket = new WebSocket("ws://localhost:8080");

    socket.onopen = function () {

        console.log("Connected to server");

        socket.send(username);
    };

    socket.onmessage = function (event) {

        console.log("Server:", event.data);

        const data = JSON.parse(event.data);

        if (data.type === "contact_list") {
            loadContacts(data.users);
        }

        else if (data.type === "user_join") {
            if (data.name !== username) {
                addContact(data.name);
            }
        }

        else if (data.type === "user_leave") {
            removeContact(data.name);
        }
    };

    socket.onclose = function () {
        console.log("Disconnected from server");
    };

    socket.onerror = function (error) {
        console.log("WebSocket error:", error);
    };
};

function loadContacts(users) {

    const container = document.getElementById("contactList");

    container.innerHTML = "";

    users.forEach(user => {

        if (user !== username) {
            addContact(user);
        }
    });
}

function addContact(name) {

    const container = document.getElementById("contactList");

    const contact = document.createElement("div");
    contact.className = "contact";
    contact.setAttribute("data-name", name);

    contact.innerHTML = `
        <div class="dp">${name[0].toUpperCase()}</div>
        <div class="contact-info">
            <div class="contact-name">${name}</div>
            <div class="description">Online</div>
        </div>
    `;

    container.appendChild(contact);
}

function removeContact(name) {

    const contact = document.querySelector(`[data-name="${name}"]`);

    if (contact) {
        contact.remove();
    }
}