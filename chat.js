let socket;
let username;
let currentContact = null;
let chatHistory = {};

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
        else if (data.type === "message") {
            const sender = data.from;
            const text = data.text;
            if (!chatHistory[sender]) {
                chatHistory[sender] = [];
            }
            chatHistory[sender].push({
                from: sender,
                text: text
            });
            if (currentContact === sender) {
                renderMessages(sender);
            }
        }
    };
    socket.onclose = function () {
        console.log("Disconnected from server");
    };
    socket.onerror = function (error) {
        console.log("WebSocket error:", error);
    };
    document.getElementById("sendBtn").onclick = sendMessage;
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
    contact.onclick = function () {
        openChat(name);
    };
    container.appendChild(contact);
}

function removeContact(name) {

    const contact = document.querySelector(`[data-name="${name}"]`);
    if (contact) {
        contact.remove();
    }
}

function openChat(name) {

    currentContact = name;
    document.getElementById("chatHeader").style.display = "block";
    document.getElementById("messageInput").style.display = "flex";
    document.getElementById("chatHeader").innerText = name;
    if (!chatHistory[name]) {
        chatHistory[name] = [];
    }
    renderMessages(name);
}

function renderMessages(name) {

    const container = document.getElementById("messages");
    container.innerHTML = "";
    chatHistory[name].forEach(msg => {
        const div = document.createElement("div");
        div.classList.add("message");
        if (msg.from === username) {
            div.classList.add("sent");
        } 
        else {
            div.classList.add("received");
        }
        div.innerText = msg.text;
        container.appendChild(div);
    });
    container.scrollTop = container.scrollHeight;
}

function sendMessage() {

    if (!currentContact) return;
    const box = document.getElementById("messageBox");
    const text = box.value.trim();
    if (text === "") 
        return;
    if (!chatHistory[currentContact]) {
        chatHistory[currentContact] = [];
    }
    chatHistory[currentContact].push({
        from: username,
        text: text
    });
    renderMessages(currentContact);
    const msg = {
        type: "message",
        to: currentContact,
        text: text
    };
    socket.send(JSON.stringify(msg));
    box.value = "";
}